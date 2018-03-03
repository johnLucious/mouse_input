#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>
#include <math.h>
#include <ncurses.h>
#include <iostream>
#include <vector>

#include <linux/input.h>
#include <thread>

#define HEIGHT 500
#define WIDTH 800
#define INIT_HEIGHT 100
#define INIT_WIDTH 100

struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;

char *fbp = 0;
int fbfd = 0;
long int screensize = 0;
long int location = 0;

typedef struct{
    int x, y;
}point;

typedef struct{
    point point1; //koord. titik awal
    point point2; //koord. titik akhir
}line;

typedef struct{
	point coorMouse;
	int clicked;
}Mice;
typedef struct {
    int cR;
    int cG;
    int cB;
} RGB;

typedef struct {
    int fd;
    Mice mice;
} FdMice;

void swap(int* a, int* b);
void printPixel(int x, int y, int colorR, int colorG, int colorB);
point setPoint(int x, int y);
void drawLine(point p1, point p2, int thickness, int colorR, int colorG, int colorB);
FdMice mouseController();
void setPointer(int x, int y);
void startDevice();
void init();
void getPixelColor(int x, int y, int *rColor, int *gColor, int *bColor);
void clearScreen();


RGB color_map[HEIGHT][WIDTH];
int main(){
    for(int i=0;i<HEIGHT;i++){
        for(int j=0;j<WIDTH;j++) {
            color_map[i][j].cR = 255;
            color_map[i][j].cG = 255;
            color_map[i][j].cB = 255;
        }
    }
	init();
	clearScreen();
	startDevice();
	int xMouse = 0, yMouse = 0;
	while(1){
        FdMice mouseCont = mouseController();
        Mice pMouse = mouseCont.mice;
        int fd = mouseCont.fd;
        clearScreen();
		xMouse = (xMouse+pMouse.coorMouse.x > WIDTH) ?WIDTH :((xMouse+pMouse.coorMouse.x < 0) ?0 :(xMouse+pMouse.coorMouse.x));
		yMouse = (yMouse+pMouse.coorMouse.y > HEIGHT) ?HEIGHT :((yMouse+pMouse.coorMouse.y < 0) ?0 :(yMouse+pMouse.coorMouse.y));
		setPointer(xMouse, yMouse);
        if(pMouse.clicked==1 && yMouse >=0 && yMouse< HEIGHT && xMouse >=0 && yMouse < WIDTH) {
            color_map[yMouse][xMouse].cR = 0;
            color_map[yMouse][xMouse].cG = 0;
            color_map[yMouse][xMouse].cB = 0;
        } 
        close(fd);
        // printf("%d %d %d\n", xMouse, yMouse, pMouse.clicked);
	}
}

void setPointer(int x, int y){
	point p1x = setPoint((x <= 5) ?0 :(x-5), y);
	point p2x = setPoint((x >= WIDTH-5) ?WIDTH :(x+5), y);
	point p1y = setPoint(x, (y <= 5) ?0 :(y-5));
	point p2y = setPoint(x, (y >= HEIGHT-5) ?HEIGHT :(y+5));
	
	drawLine(p1x, p2x, 2, 0, 0, 0);
	drawLine(p1y, p2y, 2, 0, 0, 0);
}

point setPoint(int x, int y){
    point p;
    p.x = x;
    p.y = y;

    return p;
}

FdMice mouseController(){
	int fd, bytes;
	unsigned char data[3];
	const char *mDevice = "/dev/input/mice";	//~ input mouse driver
	
	fd = open(mDevice, O_RDWR);
	if (fd == -1){
		printf("Hello Error");
		//~ return -1;
	}
	int click;
	signed char x, y;
	
	bytes = read(fd, data, sizeof(data));
	if (bytes > 0){
		click = data[0] & 0x1;				//~ Left click
		x = data[1];						//~ x coordinate
		y = data[2];						//~ y coordinate
		//~ printf("x=%d, y=%d, click=%d\n", x, y, click);
	}
	Mice M;
	M.coorMouse = setPoint(x, -y);
	M.clicked = click; 
    FdMice fdmice;
    fdmice.mice = M;
    fdmice.fd = fd;
    return (fdmice);
}

void swap(int* a, int* b){
    int temp = *a;
    *a = *b;
    *b = temp;
}

void startDevice(){
	std::thread tMouse (mouseController);
	tMouse.detach();
}

void init(){
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Error: cannot open framebuffer device");
        exit(1);
    }
    printf("The framebuffer device was opened successfully.\n");

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Error reading fixed information");
        exit(2);
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Error reading variable information");
        exit(3);
    }

    printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    // Figure out the size of the screen in bytes
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

    // Map the device to memory
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (atoi(fbp) == -1) {
        perror("Error: failed to map framebuffer device to memory");
        exit(4);
    }
    printf("The framebuffer device was mapped to memory successfully.\n");

}

void drawLine(point p1, point p2, int thickness, int colorR, int colorG, int colorB){//Bresenham
    int x1 = p1.x, y1 = p1.y, x2 = p2.x, y2 = p2.y;
    int steep = 0;
    if(abs(x1-x2) < abs(y1-y2)){
        swap(&x1, &y1);
        swap(&x2, &y2);
        steep = 1;
    }
    if(x1 > x2){
        swap(&x1,&x2);
        swap(&y1,&y2);
    }
    int dx = x2-x1;
    int dy = y2-y1;
    int derr = 2 * abs(dy);
    int err = 0;
    int y = y1;
    for(int x = x1; x <= x2; x++){
        if(steep){
            printPixel((y > WIDTH) ?WIDTH-1 :((y < 0) ?0 :y),(x > HEIGHT) ?HEIGHT-1 :((x < 0) ?0 :x),colorR,colorG,colorB);
        }else{
            printPixel((x > WIDTH) ?WIDTH-1 :((x < 0) ?0 :x),(y > HEIGHT) ?HEIGHT-1 :((y < 0) ?0 :y),colorR,colorG,colorB);
        }

        err+=derr;
        if(err > dx){
			if (y >= 0 && y < HEIGHT){
				y += (y2>y1)?1:-1;
			}
			err -= 2 * dx;
        }
    }
}

void clearScreen() {
    for (int h = 0; h < HEIGHT; h++){
        for (int w = 0; w < WIDTH; w++) {
			printPixel(w,h,color_map[h][w].cR,color_map[h][w].cG,color_map[h][w].cB);
        }
    }
}

void getPixelColor(int x, int y, int *rColor, int *gColor, int *bColor) {
      location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) +
                       (y+vinfo.yoffset) * finfo.line_length;
            *bColor = *(fbp+location);
            *gColor = *(fbp+location+1);
            *rColor = *(fbp+location+2);
}

void printPixel(int x, int y, int colorR, int colorG, int colorB){	//Print Pixel Color using RGB
    location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) +
                       (y+vinfo.yoffset) * finfo.line_length;

    if (vinfo.bits_per_pixel == 32) {
        *(fbp + location) = colorB;			//Blue Color
        *(fbp + location + 1) = colorG;		//Green Color
        *(fbp + location + 2) = colorR;		//Red Color
        *(fbp + location + 3) = 0;			//Transparancy
    }
}
