/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/log.h"

#include "nacl_io/kernel_wrap_real.h"

#include <alloca.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void nacl_io_log(const char* format, ...) {
  va_list args;
  size_t wrote;
  char* output;

#ifdef _MSC_VER
  /* TODO(sbc): vsnprintf on win32 does not return the
   * size of the buffer needed.  This can be implemented
   * on win32 in terms of _vscprintf; */
#error "not implemented for win32"
#endif

  va_start(args, format);
  int len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  output = alloca(len + 1);

  va_start(args, format);
  vsnprintf(output, len + 1, format, args);
  va_end(args);

  _real_write(2, output, strlen(output), &wrote);
}
