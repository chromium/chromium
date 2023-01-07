/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSSTAT_H_
#define LIBRARIES_NACL_IO_OSSTAT_H_

#include <sys/stat.h>

#if defined(WIN32)
#define S_IFCHR _S_IFCHR
#define S_IFDIR _S_IFDIR
#define S_IFIFO _S_IFIFO
#define S_IFREG _S_IFREG
#define S_IFMT _S_IFMT
#define S_IFSOCK _S_IFIFO

#define S_IREAD _S_IREAD
#define S_IWRITE _S_IWRITE
#define S_IEXEC _S_IEXEC
#endif

#endif  // LIBRARIES_NACL_IO_OSSTAT_H_
