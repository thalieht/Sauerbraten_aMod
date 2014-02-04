#include "engine.h"

static inline bool htcmp(const char *key, const font &f) { return !strcmp(key, f.name); }

static hashset<font> fonts;
static font *fontdef = NULL;
static int fontdeftex = 0;

font *curfont = NULL;
int curfonttex = 0;

void newfont(char *name, char *tex, int *defaultw, int *defaulth)
{
    font *f = &fonts[name];
    if(!f->name) f->name = newstring(name);
    f->texs.shrink(0);
    f->texs.add(textureload(tex));
    f->chars.shrink(0);
    f->charoffset = '!';
    f->defaultw = *defaultw;
    f->defaulth = *defaulth;
    f->scale = f->defaulth;

    fontdef = f;
    fontdeftex = 0;
}

void fontoffset(char *c)
{
    if(!fontdef) return;

    fontdef->charoffset = c[0];
}

void fontscale(int *scale)
{
    if(!fontdef) return;

    fontdef->scale = *scale > 0 ? *scale : fontdef->defaulth;
}

void fonttex(char *s)
{
    if(!fontdef) return;

    Texture *t = textureload(s);
    loopv(fontdef->texs) if(fontdef->texs[i] == t) { fontdeftex = i; return; }
    fontdeftex = fontdef->texs.length();
    fontdef->texs.add(t);
}

void fontchar(int *x, int *y, int *w, int *h, int *offsetx, int *offsety, int *advance)
{
    if(!fontdef) return;

    font::charinfo &c = fontdef->chars.add();
    c.x = *x;
    c.y = *y;
    c.w = *w ? *w : fontdef->defaultw;
    c.h = *h ? *h : fontdef->defaulth;
    c.offsetx = *offsetx;
    c.offsety = *offsety;
    c.advance = *advance ? *advance : c.offsetx + c.w;
    c.tex = fontdeftex;
}

void fontskip(int *n)
{
    if(!fontdef) return;
    loopi(max(*n, 1))
    {
        font::charinfo &c = fontdef->chars.add();
        c.x = c.y = c.w = c.h = c.offsetx = c.offsety = c.advance = c.tex = 0;
    }
}

COMMANDN(font, newfont, "ssii");
COMMAND(fontoffset, "s");
COMMAND(fontscale, "i");
COMMAND(fonttex, "s");
COMMAND(fontchar, "iiiiiii");
COMMAND(fontskip, "i");

void fontalias(const char *dst, const char *src)
{
    font *s = fonts.access(src);
    if(!s) return;
    font *d = &fonts[dst];
    if(!d->name) d->name = newstring(dst);
    d->texs = s->texs;
    d->chars = s->chars;
    d->charoffset = s->charoffset;
    d->defaultw = s->defaultw;
    d->defaulth = s->defaulth;
    d->scale = s->scale;

    fontdef = d;
    fontdeftex = d->texs.length()-1;
}

COMMAND(fontalias, "ss");

bool setfont(const char *name)
{
    font *f = fonts.access(name);
    if(!f) return false;
    curfont = f;
    return true;
}

static vector<font *> fontstack;

void pushfont()
{
    fontstack.add(curfont);
}

bool popfont()
{
    if(fontstack.empty()) return false;
    curfont = fontstack.pop();
    return true;
}

void gettextres(int &w, int &h)
{
    if(w < MINRESW || h < MINRESH)
    {
        if(MINRESW > w*MINRESH/h)
        {
            h = h*MINRESW/w;
            w = MINRESW;
        }
        else
        {
            w = w*MINRESH/h;
            h = MINRESH;
        }
    }
}

float text_widthf(const char *str)
{
    float width, height;
    text_boundsf(str, width, height);
    return width;
}

#define FONTTAB (4*FONTW)
#define TEXTTAB(x) ((int((x)/FONTTAB)+1.0f)*FONTTAB)

void tabify(const char *str, int *numtabs)
{
    int tw = max(*numtabs, 0)*FONTTAB-1, tabs = 0;
    for(float w = text_widthf(str); w <= tw; w = TEXTTAB(w)) ++tabs;
    int len = strlen(str);
    char *tstr = newstring(len + tabs);
    memcpy(tstr, str, len);
    memset(&tstr[len], '\t', tabs);
    tstr[len+tabs] = '\0';
    stringret(tstr);
}

COMMAND(tabify, "si");

void draw_textf(const char *fstr, int left, int top, ...)
{
    defvformatstring(str, top, fstr);
    draw_text(str, left, top);
}

static float draw_char(Texture *&tex, int c, float x, float y, float scale)
{
    font::charinfo &info = curfont->chars[c-curfont->charoffset];
    if(tex != curfont->texs[info.tex])
    {
        xtraverts += varray::end();
        tex = curfont->texs[info.tex];
        glBindTexture(GL_TEXTURE_2D, tex->id);
    }

    float x1 = x + scale*info.offsetx,
          y1 = y + scale*info.offsety,
          x2 = x + scale*(info.offsetx + info.w),
          y2 = y + scale*(info.offsety + info.h),
          tx1 = info.x / float(tex->xs),
          ty1 = info.y / float(tex->ys),
          tx2 = (info.x + info.w) / float(tex->xs),
          ty2 = (info.y + info.h) / float(tex->ys);

    varray::attrib<float>(x1, y1); varray::attrib<float>(tx1, ty1);
    varray::attrib<float>(x2, y1); varray::attrib<float>(tx2, ty1);
    varray::attrib<float>(x2, y2); varray::attrib<float>(tx2, ty2);
    varray::attrib<float>(x1, y2); varray::attrib<float>(tx1, ty2);

    return scale*info.advance;
}

// text color variables controlled by gui or "/varName number" (but that would be impractical)
// Name explanation: example colorRe: color<Red or Green or Blue> <output to char 'e'> eg in cubescript ^fe
VARP(colorRq, 0, 1   ,255);
VARP(colorGq, 0, 255 ,255);
VARP(colorBq, 0, 255 ,255);
VARP(colorRw, 0, 1   ,255);
VARP(colorGw, 0, 255 ,255);
VARP(colorBw, 0, 255 ,255);
VARP(colorRe, 0, 255 ,255);
VARP(colorGe, 0, 13  ,255);
VARP(colorBe, 0, 170 ,255);
VARP(colorRz, 0, 1   ,255);
VARP(colorGz, 0, 255 ,255);
VARP(colorBz, 0, 255 ,255);
VARP(colorRt, 0, 1   ,255);
VARP(colorGt, 0, 255 ,255);
VARP(colorBt, 0, 255 ,255);
VARP(colorRa, 0, 255 ,255);
VARP(colorGa, 0, 255 ,255);
VARP(colorBa, 0, 1   ,255);
VARP(colorRx, 0, 255 ,255);
VARP(colorGx, 0, 255 ,255);
VARP(colorBx, 0, 255 ,255);
VARP(colorRu, 0, 64  ,255);
VARP(colorGu, 0, 255 ,255);
VARP(colorBu, 0, 128 ,255);
VARP(colorRv, 0, 64  ,255);
VARP(colorGv, 0, 255 ,255);
VARP(colorBv, 0, 128 ,255);
VARP(colorRk, 0, 50  ,255);
VARP(colorGk, 0, 139 ,255);
VARP(colorBk, 0, 50  ,255);
VARP(colorRd, 0, 169 ,255);
VARP(colorGd, 0, 139 ,255);
VARP(colorBd, 0, 50  ,255);
VARP(colorRf, 0, 10  ,255);
VARP(colorGf, 0, 162 ,255);
VARP(colorBf, 0, 255 ,255);

//stack[sp] is current color index
static void text_color(char c, char *stack, int size, int &sp, bvec color, int a)
{
    if(c=='s') // save color
    {
        c = stack[sp];
        if(sp<size-1) stack[++sp] = c;
    }
    else
    {
        xtraverts += varray::end();
        if(c=='r') c = stack[(sp > 0) ? --sp : sp]; // restore color
        else stack[sp] = c;
        switch(c)
        {
            case '0': color = bvec( 64, 255, 128); break;   // green: player talk
            case '1': color = bvec( 96, 160, 255); break;   // blue: "echo" command
            case '2': color = bvec(255, 192,  64); break;   // yellow: gameplay messages
            case '3': color = bvec(255,  64,  64); break;   // red: important errors
            case '4': color = bvec(128, 128, 128); break;   // gray
            case '5': color = bvec(192,  64, 192); break;   // magenta
            case '6': color = bvec(255, 128,   0); break;   // orange
            case '7': color = bvec(255, 255, 255); break;   // white
            case '8': color = bvec(255,  55, 155); break;   // pink
            case '9': color = bvec( 50, 255, 255); break;   // cyan
            case 'b': color = bvec(  0, 110, 255); break;   // new blue
            case 'g': color = bvec(  0, 255,   0); break;   // new green
            case 'h': color = bvec(255, 215,   0); break;   // dark green
            case 'm': color = bvec(200,   0, 255); break;   // new magenta
            case 'o': color = bvec(255, 148,   0); break;   // new orange
            case 'p': color = bvec(255,   0,   0); break;   // new red
            case 'y': color = bvec(255, 255,   0); break;   // new yellow
            case 'q': color = bvec(colorRq, colorGq, colorBq); break;   // adjustable ingame
            case 'w': color = bvec(colorRw, colorGw, colorBw); break;   // adjustable ingame
            case 'e': color = bvec(colorRe, colorGe, colorBe); break;   // adjustable ingame
            case 'z': color = bvec(colorRz, colorGz, colorBz); break;   // adjustable ingame
            case 't': color = bvec(colorRt, colorGt, colorBt); break;   // adjustable ingame
            case 'a': color = bvec(colorRa, colorGa, colorBa); break;   // adjustable ingame
            case 'x': color = bvec(colorRx, colorGx, colorBx); break;   // adjustable ingame
            case 'u': color = bvec(colorRu, colorGu, colorBu); break;   // adjustable ingame
            case 'v': color = bvec(colorRv, colorGv, colorBv); break;   // adjustable ingame
            case 'k': color = bvec(colorRk, colorGk, colorBk); break;   // adjustable ingame
            case 'd': color = bvec(colorRd, colorGd, colorBd); break;   // adjustable ingame
            case 'f': color = bvec(colorRf, colorGf, colorBf); break;   // adjustable ingame
            // provided color: everything else
        }
        glColor4ub(color.x, color.y, color.z, a);
    }
}

#define TEXTSKELETON \
    float y = 0, x = 0, scale = curfont->scale/float(curfont->defaulth);\
    int i;\
    for(i = 0; str[i]; i++)\
    {\
        TEXTINDEX(i)\
        int c = uchar(str[i]);\
        if(c=='\t')      { x = TEXTTAB(x); TEXTWHITE(i) }\
        else if(c==' ')  { x += scale*curfont->defaultw; TEXTWHITE(i) }\
        else if(c=='\n') { TEXTLINE(i) x = 0; y += FONTH; }\
        else if(c=='\f') { if(str[i+1]) { i++; TEXTCOLOR(i) }}\
        else if(curfont->chars.inrange(c-curfont->charoffset))\
        {\
            float cw = scale*curfont->chars[c-curfont->charoffset].advance;\
            if(cw <= 0) continue;\
            if(maxwidth != -1)\
            {\
                int j = i;\
                float w = cw;\
                for(; str[i+1]; i++)\
                {\
                    int c = uchar(str[i+1]);\
                    if(c=='\f') { if(str[i+2]) i++; continue; }\
                    if(i-j > 16) break;\
                    if(!curfont->chars.inrange(c-curfont->charoffset)) break;\
                    float cw = scale*curfont->chars[c-curfont->charoffset].advance;\
                    if(cw <= 0 || w + cw > maxwidth) break;\
                    w += cw;\
                }\
                if(x + w > maxwidth && j!=0) { TEXTLINE(j-1) x = 0; y += FONTH; }\
                TEXTWORD\
            }\
            else\
            { TEXTCHAR(i) }\
        }\
    }

//all the chars are guaranteed to be either drawable or color commands
#define TEXTWORDSKELETON \
                for(; j <= i; j++)\
                {\
                    TEXTINDEX(j)\
                    int c = uchar(str[j]);\
                    if(c=='\f') { if(str[j+1]) { j++; TEXTCOLOR(j) }}\
                    else { float cw = scale*curfont->chars[c-curfont->charoffset].advance; TEXTCHAR(j) }\
                }

#define TEXTEND(cursor) if(cursor >= i) { do { TEXTINDEX(cursor); } while(0); }

int text_visible(const char *str, float hitx, float hity, int maxwidth)
{
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx) if(y+FONTH > hity && x >= hitx) return idx;
    #define TEXTLINE(idx) if(y+FONTH > hity) return idx;
    #define TEXTCOLOR(idx)
    #define TEXTCHAR(idx) x += cw; TEXTWHITE(idx)
    #define TEXTWORD TEXTWORDSKELETON
    TEXTSKELETON
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
    return i;
}

//inverse of text_visible
void text_posf(const char *str, int cursor, float &cx, float &cy, int maxwidth)
{
    #define TEXTINDEX(idx) if(idx == cursor) { cx = x; cy = y; break; }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx)
    #define TEXTCOLOR(idx)
    #define TEXTCHAR(idx) x += cw;
    #define TEXTWORD TEXTWORDSKELETON if(i >= cursor) break;
    cx = cy = 0;
    TEXTSKELETON
    TEXTEND(cursor)
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
}

void text_boundsf(const char *str, float &width, float &height, int maxwidth)
{
    #define TEXTINDEX(idx)
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) if(x > width) width = x;
    #define TEXTCOLOR(idx)
    #define TEXTCHAR(idx) x += cw;
    #define TEXTWORD x += w;
    width = 0;
    TEXTSKELETON
    height = y + FONTH;
    TEXTLINE(_)
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
}

void draw_text(const char *str, int left, int top, int r, int g, int b, int a, int cursor, int maxwidth)
{
    #define TEXTINDEX(idx) if(idx == cursor) { cx = x; cy = y; }
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx)
    #define TEXTCOLOR(idx) if(usecolor) text_color(str[idx], colorstack, sizeof(colorstack), colorpos, color, a);
    #define TEXTCHAR(idx) draw_char(tex, c, left+x, top+y, scale); x += cw;
    #define TEXTWORD TEXTWORDSKELETON
    char colorstack[10];
    colorstack[0] = 'c'; //indicate user color
    bvec color(r, g, b);
    int colorpos = 0;
    float cx = -FONTW, cy = 0;
    bool usecolor = true;
    if(a < 0) { usecolor = false; a = -a; }
    Texture *tex = curfont->texs[0];
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glColor4ub(color.x, color.y, color.z, a);
    varray::enable();
    varray::defattrib(varray::ATTRIB_VERTEX, 2, GL_FLOAT);
    varray::defattrib(varray::ATTRIB_TEXCOORD0, 2, GL_FLOAT);
    varray::begin(GL_QUADS);
    TEXTSKELETON
    TEXTEND(cursor)
    xtraverts += varray::end();
    if(cursor >= 0 && (totalmillis/250)&1)
    {
        glColor4ub(r, g, b, a);
        if(maxwidth != -1 && cx >= maxwidth) { cx = 0; cy += FONTH; }
        draw_char(tex, '_', left+cx, top+cy, scale);
        xtraverts += varray::end();
    }
    varray::disable();
    #undef TEXTINDEX
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
}

void reloadfonts()
{
    enumerate(fonts, font, f,
        loopv(f.texs) if(!reloadtexture(*f.texs[i])) fatal("failed to reload font texture");
    );
}

