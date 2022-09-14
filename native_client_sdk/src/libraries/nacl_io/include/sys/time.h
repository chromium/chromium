/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include_next <sys/time.h>

#ifndef LIBRARIES_NACL_IO_INCLUDE_SYS_TIME_H_
#define LIBRARIES_NACL_IO_INCLUDE_SYS_TIME_H_

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

// TODO(binji): https://code.google.com/p/nativeclient/issues/detail?id=3949
// remove when these declarations are added to the newlib headers.
int utimes(const char *filename, const struct timeval times[2]);
int futimes(int fd, const struct timeval times[2]);

__END_DECLS

#endif  // LIBRARIES_NACL_IO_INCLUDE_SYS_TIME_H_
