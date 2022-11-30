/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSSIGNAL_H_
#define LIBRARIES_NACL_IO_OSSIGNAL_H_

#if !defined(WIN23)
#include <signal.h>

#if defined(__APPLE__)
typedef void (*sighandler_t)(int);
#elif defined(__GLIBC__) || defined(__BIONIC__)
typedef __sighandler_t sighandler_t;
#else
typedef _sig_func_ptr sighandler_t;
#endif
#endif /*  !WIN23 */

#endif  // LIBRARIES_NACL_IO_OSSIGNAL_H_
