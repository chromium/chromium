/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdarg.h>

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

int fcntl(int fd, int cmd, ...) {
  va_list ap;
  va_start(ap, cmd);
  int rtn = ki_fcntl(fd, cmd, ap);
  va_end(ap);
  return rtn;
}
