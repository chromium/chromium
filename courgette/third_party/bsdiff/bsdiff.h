// Copyright 2003, 2004 Colin Percival
// All rights reserved
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted providing that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// For the terms under which this work may be distributed, please see
// the adjoining file "LICENSE".
//
// Changelog:
// 2005-04-26 - Define the header as a C structure, add a CRC32 checksum to
//              the header, and make all the types 32-bit.
//                --Benjamin Smedberg <benjamin@smedbergs.us>
// 2009-03-31 - Change to use Streams.  Move CRC code to crc.{h,cc}
//              Changed status to an enum, removed unused status codes.
//                --Stephen Adams <sra@chromium.org>
// 2013-04-10 - Added wrapper to apply a patch directly to files.
//                --Joshua Pawlicki <waffles@chromium.org>

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_THIRD_PARTY_BSDIFF_BSDIFF_H_
#define COURGETTE_THIRD_PARTY_BSDIFF_BSDIFF_H_

#include <stdint.h>

#include "base/files/file.h"

namespace base {
class FilePath;
}

namespace courgette {
class SourceStream;
class SinkStream;
}  // namespace courgette

namespace bsdiff {

enum BSDiffStatus {
  OK = 0,
  MEM_ERROR = 1,
  CRC_ERROR = 2,
  READ_ERROR = 3,
  UNEXPECTED_ERROR = 4,
  WRITE_ERROR = 5
};

// Creates a binary patch.
//
BSDiffStatus CreateBinaryPatch(courgette::SourceStream* old_stream,
                               courgette::SourceStream* new_stream,
                               courgette::SinkStream* patch_stream);

// Applies the given patch file to a given source file. This method validates
// the CRC of the original file stored in the patch file, before applying the
// patch to it.
//
BSDiffStatus ApplyBinaryPatch(courgette::SourceStream* old_stream,
                              courgette::SourceStream* patch_stream,
                              courgette::SinkStream* new_stream);

// As above, but simply takes base::Files.
BSDiffStatus ApplyBinaryPatch(base::File old_stream,
                              base::File patch_stream,
                              base::File new_stream);

// As above, but simply takes the file paths.
BSDiffStatus ApplyBinaryPatch(const base::FilePath& old_stream,
                              const base::FilePath& patch_stream,
                              const base::FilePath& new_stream);

// The following declarations are common to the patch-creation and
// patch-application code.

// The patch stream starts with a MBSPatchHeader.
typedef struct MBSPatchHeader_ {
  char tag[8];      // Contains MBS_PATCH_HEADER_TAG.
  uint32_t slen;    // Length of the file to be patched.
  uint32_t scrc32;  // CRC32 of the file to be patched.
  uint32_t dlen;    // Length of the result file.
} MBSPatchHeader;

// This is the value for the tag field.  Must match length exactly, not counting
// null at end of string.
#define MBS_PATCH_HEADER_TAG "GBSDIF42"

}  // namespace bsdiff

#endif  // COURGETTE_THIRD_PARTY_BSDIFF_BSDIFF_H_
