/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSUNISTD_H_
#define LIBRARIES_NACL_IO_OSUNISTD_H_

#if defined(WIN32)

#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

#else

#include <unistd.h>

#endif

#endif  // LIBRARIES_NACL_IO_OSUNISTD_H_
