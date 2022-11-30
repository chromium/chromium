/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSTIME_H_
#define LIBRARIES_NACL_IO_OSTIME_H_

#if defined(WIN32)

#include <pthread.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME (clockid_t)1
#endif

int clock_gettime(clockid_t clock_id, struct timespec* tp);
int clock_settime(clockid_t clock_id, const struct timespec* tp);

#else

#include <time.h>
#include <utime.h>
#include <sys/time.h>

#if defined(__GLIBC__)
#define st_atimensec st_atim.tv_nsec
#define st_mtimensec st_mtim.tv_nsec
#define st_ctimensec st_ctim.tv_nsec
#endif

#if defined(__APPLE__) && !defined(_POSIX_C_SOURCE)
#define st_atimensec st_atimespec.tv_nsec
#define st_mtimensec st_mtimespec.tv_nsec
#define st_ctimensec st_ctimespec.tv_nsec
#endif

#endif

#endif  // LIBRARIES_NACL_IO_OSTIME_H_
