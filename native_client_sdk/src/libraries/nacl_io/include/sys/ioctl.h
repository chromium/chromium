/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_INCLUDE_SYS_IOCTL_H_
#define LIBRARIES_NACL_IO_INCLUDE_SYS_IOCTL_H_

#include <sys/cdefs.h>

#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414

struct winsize {
  unsigned short ws_row;
  unsigned short ws_col;
  unsigned short ws_xpixel;
  unsigned short ws_ypixel;
};

__BEGIN_DECLS

int ioctl(int fd, unsigned long request, ...);

__END_DECLS

#endif  // LIBRARIES_NACL_IO_INCLUDE_SYS_IOCTL_H_
