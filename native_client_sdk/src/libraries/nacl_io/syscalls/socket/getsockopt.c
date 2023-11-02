/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

#if defined(PROVIDES_SOCKET_API) && !defined(NACL_GLIBC_OLD)

int getsockopt(int fd, int lvl, int optname, void* optval, socklen_t* len) {
  return ki_getsockopt(fd, lvl, optname, optval, len);
}

#endif  /* defined(PROVIDES_SOCKET_API) && !defined(NACL_GLIBC_OLD) */
