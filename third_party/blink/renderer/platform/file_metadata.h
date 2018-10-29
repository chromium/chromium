/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FILE_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FILE_METADATA_H_

#include <time.h>
#include "base/files/file.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

inline double InvalidFileTime() {
  return std::numeric_limits<double>::quiet_NaN();
}
inline bool IsValidFileTime(double time) {
  return std::isfinite(time);
}

class FileMetadata {
  DISALLOW_NEW();

 public:
  FileMetadata()
      : modification_time(InvalidFileTime()), length(-1), type(kTypeUnknown) {}

  PLATFORM_EXPORT static FileMetadata From(const base::File::Info& file_info);

  // The last modification time of the file, in milliseconds.
  // The value NaN means that the time is not known.
  double modification_time;

  // The length of the file in bytes.
  // The value -1 means that the length is not set.
  long long length;

  enum Type { kTypeUnknown = 0, kTypeFile, kTypeDirectory };

  Type type;
  String platform_path;
};

PLATFORM_EXPORT bool GetFileSize(const String&, long long& result);
PLATFORM_EXPORT bool GetFileModificationTime(const String&, double& result);
PLATFORM_EXPORT bool GetFileMetadata(const String&, FileMetadata&);
PLATFORM_EXPORT KURL FilePathToURL(const String&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FILE_METADATA_H_
