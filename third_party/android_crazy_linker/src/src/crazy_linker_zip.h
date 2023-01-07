// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ZIP_H
#define CRAZY_LINKER_ZIP_H

#include "crazy_linker_util.h"  // For CRAZY_OFFSET_FAILED

// Definitions related to supporting loading libraries from zip files.
#include <cstdint>

namespace crazy {

// Find "filename" in the specified "zip_file" and return the offset
// in the file of the start of the data for the file. Return
// CRAZY_OFFSET_FAILED on error or if the file is compressed. This routine
// replaces code which used the minizip library, but is about 150 times faster,
// locating the offset in less than 0.5ms on a Nexus 4.
int32_t FindStartOffsetOfFileInZipFile(const char* zip_file,
                                       const char* filename);
}

#endif  // CRAZY_LINKER_ZIP_H
