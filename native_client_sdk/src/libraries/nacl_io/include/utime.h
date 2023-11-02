/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include_next <utime.h>

#ifndef LIBRARIES_NACL_IO_INCLUDE_UTIME_H_
#define LIBRARIES_NACL_IO_INCLUDE_UTIME_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

// TODO(binji): https://code.google.com/p/nativeclient/issues/detail?id=3949
// remove when these declarations are added to the newlib headers.
int utime(const char* filename, const struct utimbuf* times);

__END_DECLS

#endif  // LIBRARIES_NACL_IO_INCLUDE_UTIME_H_
