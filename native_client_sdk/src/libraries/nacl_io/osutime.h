/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_OSUTIME_H_
#define LIBRARIES_NACL_IO_OSUTIME_H_

#if defined(WIN32)
#define utimbuf _utimbuf
#endif

struct utimbuf;

#endif  // LIBRARIES_NACL_IO_OSUTIME_H_
