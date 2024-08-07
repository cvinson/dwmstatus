/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

char *tzchicago = "America/Chicago";
char *tzargentina = "America/Buenos_Aires";
char *tzutc = "UTC";
char *tzberlin = "Europe/Berlin";

static Display *dpy;

char * smprintf(char *fmt, ...) {
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void settz(char *tzname) {
	setenv("TZ", tzname, 1);
}

char * mktimes(char *fmt, char *tzname) {
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void setstatus(char *str) {
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char * loadavg(void) {
	double avgs[3];

	if (getloadavg(avgs, 3) < 0)
		return smprintf("");

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char * readfile(char *base, char *file) {
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char * getvolume() {
  int MAX_LENGTH = 5;
  FILE *volume_file;
  FILE *mute_file;
  char volume[MAX_LENGTH];
  char mute[MAX_LENGTH];

  volume_file = popen("/usr/bin/pactl get-sink-volume 0 | /usr/bin/sed -n 's/.*\\b\\([0-9]\\+\\%\\) .*/\\1/p'", "r");
  mute_file = popen("/usr/bin/pactl get-sink-mute 0 | /usr/bin/sed -n 's/Mute: \\(\\w*\\)/\\1/p'", "r");

  fgets(volume, MAX_LENGTH, volume_file);
  volume[strcspn(volume, "\n")] = 0;

  fgets(mute, MAX_LENGTH, mute_file);

  pclose(volume_file);
  pclose(mute_file);

  if (strcmp(mute, "yes") > 0) {
    return "X";
  }

  return smprintf("%s", volume);
}

char * getbrightness() {
  int MAX_LENGTH = 10;
  float percentage;
  FILE *brightness_file;
  FILE *max_file;
  char brightness[MAX_LENGTH];
  char max[MAX_LENGTH];

  brightness_file = popen("/usr/bin/brightnessctl g", "r");
  max_file = popen("/usr/bin/brightnessctl m", "r");

  if (brightness_file == NULL || max_file == NULL) {
    printf("Could not read.");
  }

  fgets(brightness, MAX_LENGTH, brightness_file);
  fgets(max, MAX_LENGTH, max_file);

  percentage = ((float)atoi(brightness) / atoi(max)) * 100; 

  pclose(brightness_file);
  pclose(max_file);

  return smprintf("%0.0f%%", percentage);
}

char * getbattery(char *base) {
	char *co, status;
	int descap, remcap;
  float charge;

	descap = -1;
	remcap = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "charge_full");
	if (co == NULL) {
		co = readfile(base, "energy_full");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &descap);
	free(co);

	co = readfile(base, "charge_now");
	if (co == NULL) {
		co = readfile(base, "energy_now");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &remcap);
	free(co);

	co = readfile(base, "status");
	if (!strncmp(co, "Discharging", 11)) {
		status = '-';
	} else if(!strncmp(co, "Charging", 8)) {
		status = '+';
  } else if (!strncmp(co, "Full", 4)) {
    status = '!';
	} else {
		status = '?';
	}

  free(co);

  if (remcap < 0 || descap < 0)
    return smprintf("invalid");

  charge = ((float)remcap / (float)descap) * 100;

  if (charge > 100 || status == '!') {
    charge = 100;
  }

	return smprintf("%.0f%%%c", charge, status);
}

int main(void) {
	char *status;
	char *bat;
  char *bri;
	char *tchi;
  char *vol;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		bat = getbattery("/sys/class/power_supply/BAT0");
    bri = getbrightness();
		tchi = mktimes("%a %d %b %I:%M", tzchicago);
    vol = getvolume();

		status = smprintf("Vol:%s Bri:%s Bat:%s %s", vol, bri, bat, tchi);
		setstatus(status);

		free(bat);
    free(bri);
		free(tchi);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

