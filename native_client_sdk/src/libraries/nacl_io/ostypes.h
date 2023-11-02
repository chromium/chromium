/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSTYPES_H_
#define LIBRARIES_NACL_IO_OSTYPES_H_

#include <sys/types.h>

#if defined(WIN32)

#include <BaseTsd.h>

typedef int mode_t;
typedef SSIZE_T ssize_t;
typedef int uid_t;
typedef int gid_t;

#endif

#endif  // LIBRARIES_NACL_IO_OSTYPES_H_
