/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

#if defined(PROVIDES_SOCKET_API) && !defined(NACL_GLIBC_OLD)

ssize_t recv(int fd, void* buf, size_t len, int flags) {
  return ki_recv(fd, buf, len, flags);
}

#endif  /* defined(PROVIDES_SOCKET_API) && !defined(NACL_GLIBC_OLD) */
