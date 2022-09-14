/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <sys/types.h>
/*
 * Include something that will define __BIONIC__, then wrap the entire file
 * in this #if, so this file will be compiled on a non-bionic build.
 */

#if !defined(__BIONIC__)
#include <errno.h>

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

int tcflow(int fd, int action) {
  errno = ENOSYS;
  return -1;
}

#endif /* #if !defined(__BIONIC_) */