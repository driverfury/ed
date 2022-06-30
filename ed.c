/*** Defines and macros ***/

#define IS_DIGIT(c) ((c)>='0'&&(c)<='9')
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define MIN(x, y) ((x)<(y)?(x):(y))
#define MAX(x, y) ((x)>(y)?(x):(y))

/* TODO: Is this platform independent? */
#define CTRL_KEY(k) ((k)&0x1f)

enum TermInitCode
{
    TERM_INIT_SUCCESS,
    TERM_INIT_FAIL
};

/*** Utils ***/

/*** Platform dependent stuff ***/

enum TermInitCode   term_init(void);
void                term_reset(void);
int                 term_read(void);
void                term_write(int c);

#ifdef LINUX

#include <termios.h>
#include <unistd.h>

struct termios termios_orig;
int ttyfd;

enum TermInitCode linux_init(void)
{
    ttyfd = STDIN_FILENO;

    if(!isatty(ttyfd))
    {
        return(TERM_INIT_FAIL);
    }

    if(tcgetattr(ttyfd, &termios_orig) < 0)
    {
        return(TERM_INIT_FAIL);
    }

    return(TERM_INIT_SUCCESS);
}

enum TermInitCode linux_raw_mode(void)
{
    struct termios termios_raw;

    termios_raw = termios_orig;
    termios_raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    termios_raw.c_oflag &= ~(OPOST);
    termios_raw.c_cflag |= (CS8);
    termios_raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    termios_raw.c_cc[VMIN] = 0;
    termios_raw.c_cc[VTIME] = 0;

    if(tcsetattr(ttyfd, TCSAFLUSH, &termios_raw) < 0)
    {
        return(TERM_INIT_FAIL);
    }

    return(TERM_INIT_SUCCESS);
}

enum TermInitCode term_init(void)
{
    enum TermInitCode res;

    res = linux_init();

    if(res != TERM_INIT_SUCCESS)
    {
        return(res);
    }

    res = linux_raw_mode();

    return(res);
}

void term_reset(void)
{
    tcsetattr(ttyfd, TCSAFLUSH, &termios_orig);
}

int term_read(void)
{
    int cin;

    cin = 0;
    while(read(ttyfd, &cin, 1) <= 0);

    return(cin);
}

void term_write(int c)
{
    char cout;

    cout = (char)c;
    write(ttyfd, &cout, 1);
}

#else

#error "No platform specified"

#endif

/*** Terminal (VT100) ***/

/* snprintf */
#include <stdio.h>

#define TERM_VT100_ESC "\x1b["
#define TERM_VT100_CLR "2J"
#define TERM_VT100_CURS_SET "H"
#define TERM_VT100_CURS_GET "6n"

void term_writes(char *s)
{
    while(*s)
    {
        term_write((int)*s);
        ++s;
    }
}

void term_clear(void)
{
    term_writes(TERM_VT100_ESC TERM_VT100_CLR);
}

void term_cursor_set(int x, int y)
{
    char buf[64];

    snprintf(buf, ARRAY_COUNT(buf),
            TERM_VT100_ESC "%d;%d" TERM_VT100_CURS_SET,
            y, x);
    buf[ARRAY_COUNT(buf) - 1] = 0;

    term_writes(buf);
}

void term_cursor_get(int *x, int *y)
{
    char buf[64];
    char *s;
    int i, cin;

    term_writes(TERM_VT100_ESC TERM_VT100_CURS_GET);

    *x = *y = 0;
    i = 0;
    while(i < ARRAY_COUNT(buf))
    {
        cin = term_read();
        buf[i++] = cin;
        if(cin == 'R')
        {
            break;
        }
    }
    buf[i] = 0;

    if(buf[0] == '\x1b' && buf[1] == '[')
    {
        s = buf + 2;
        sscanf(s, "%d;%d", y, x);
    }
}

void term_size_get(int *x, int *y)
{
    int cx, cy;

    term_cursor_get(&cx, &cy);
    term_cursor_set(9999, 9999);
    term_cursor_get(x, y);
    *x += 1;
    *y += 1;
    term_cursor_set(cx, cy);
}

/*** Editor ***/

struct Editor
{
    int w, h;
    int cx, cy;
};

void editor_init(struct Editor *ed)
{
    term_clear();
    term_size_get(&ed->w, &ed->h);
    term_cursor_set(0, 0);
}

void editor_update(struct Editor *ed)
{
    term_cursor_set(ed->cx, ed->cy);
}

/*** Main ***/

struct Editor ed;

int main(int argc, char *argv[])
{
    int running;

    if(term_init() == TERM_INIT_SUCCESS)
    {
        running = 1;
    }
    else
    {
        term_reset();
        running = 0;
        return(1);
    }

    editor_init(&ed);
    while(running)
    {
        int cin;

        cin = term_read();
        switch(cin)
        {
            case 'q':
            {
                running = 0;
            } break;

            case 'h': { ed.cx = MAX(ed.cx - 1, 0); } break;
            case 'l': { ed.cx = MIN(ed.cx + 1, ed.w - 1); } break;
            case 'k': { ed.cy = MAX(ed.cy - 1, 0); } break;
            case 'j': { ed.cy = MIN(ed.cy + 1, ed.h - 1); } break;
        }

        editor_update(&ed);
    }

    term_reset();
    return(0);
}
