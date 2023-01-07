/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSINTTYPES_H_
#define LIBRARIES_NACL_IO_OSINTTYPES_H_

/* Define printf/scanf format strings for size_t. */

#if defined(WIN32)

#if !defined(PRIuS)
#define PRIuS "Iu"
#endif

#if !defined(SCNuS)
#define SCNuS "Iu"
#endif

#else

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>

#if !defined(PRIuS)
#define PRIuS "zu"
#endif

#if !defined(SCNuS)
#define SCNuS "zu"
#endif

#endif

#if !defined(__native_client__) && !defined(__APPLE__)
#define PRIoff "ld"
#else
#define PRIoff "lld"
#endif

#endif  // LIBRARIES_NACL_IO_OSINTTYPES_H_
