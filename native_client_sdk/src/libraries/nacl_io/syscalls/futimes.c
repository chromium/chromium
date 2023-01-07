/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

int futimes(int fd, const struct timeval times[2]) {
  return ki_futimes(fd, times);
}

