/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSSOCKET_H_
#define LIBRARIES_NACL_IO_OSSOCKET_H_

#include <sys/types.h>

#if !defined(WIN32)
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/select.h>

#if defined(_NEWLIB_VERSION)
struct iovec {
  void  *iov_base;
  size_t iov_len;
};
#endif

#define PROVIDES_SOCKET_API
#endif

#endif  // LIBRARIES_NACL_IO_OSSOCKET_H_
