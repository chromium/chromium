/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

#if !defined(__BIONIC__)

sighandler_t signal(int signum, sighandler_t handler) {
  return ki_signal(signum, handler);
}

#else

sighandler_t sysv_signal(int signum, sighandler_t handler)
{
  return ki_signal(signum, handler);
}


sighandler_t bsd_signal(int signum, sighandler_t handler) {
  return ki_signal(signum, handler);
}

#endif /* #if !defined(__BIONIC_) */