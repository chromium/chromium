/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSTERMIOS_H_
#define LIBRARIES_NACL_IO_OSTERMIOS_H_

#if defined(WIN32)

#include "sdk_util/macros.h"

typedef unsigned char cc_t;
typedef unsigned short tcflag_t;
typedef char speed_t;

#define NCCS 32
struct termios {
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;
  char c_line;
  cc_t c_cc[NCCS];
  speed_t c_ispeed;
  speed_t c_ospeed;
};

EXTERN_C_BEGIN

int tcgetattr(int fd, struct termios* termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios* termios_p);

EXTERN_C_END

#else

#include <termios.h>

#endif


#endif  // LIBRARIES_NACL_IO_OSTERMIOS_H_
