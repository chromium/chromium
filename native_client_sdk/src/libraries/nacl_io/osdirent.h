/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSDIRENT_H_
#define LIBRARIES_NACL_IO_OSDIRENT_H_

#if defined(WIN32)

#include <sys/types.h>
#include "sdk_util/macros.h"

struct dirent {
  _ino_t d_ino;
  _off_t d_off;
  unsigned short int d_reclen;
  char d_name[256];
};

#else

#include <sys/types.h>
#include <dirent.h>

#endif

#endif  // LIBRARIES_NACL_IO_OSDIRENT_H_
