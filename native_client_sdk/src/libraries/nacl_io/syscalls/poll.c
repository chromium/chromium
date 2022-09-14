/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

#if !defined(__BIONIC__)

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
  return ki_poll(fds, nfds, timeout);
}

#endif /* #if !defined(__BIONIC_) */