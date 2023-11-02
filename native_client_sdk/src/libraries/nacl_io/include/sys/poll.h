/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_INCLUDE_SYS_POLL_H_
#define LIBRARIES_NACL_IO_INCLUDE_SYS_POLL_H_

#include <stdint.h>
#include <sys/cdefs.h>

/* This header adds definitions of flags and structures for use with poll on
 * toolchains with 'C' libraries which do not normally supply poll. */

/* Node state flags */
#define POLLIN   0x0001   /* Will not block READ select/poll. */
#define POLLPRI  0x0002   /* There is urgent data to read. */
#define POLLOUT  0x0004   /* Will not block WRITE select/poll. */
#define POLLERR  0x0008   /* Will not block EXECPT select/poll. */
#define POLLHUP  0x0010   /* Connection closed on far side. */
#define POLLNVAL 0x0020   /* Invalid FD. */

/* Number of file descriptors. */
typedef int nfds_t;

struct pollfd {
  int fd;
  uint16_t events;
  uint16_t revents;
};

__BEGIN_DECLS

int poll(struct pollfd* __fds, nfds_t __nfds, int __timeout);

__END_DECLS

#endif  // LIBRARIES_NACL_IO_INCLUDE_SYS_POLL_H_
