// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_stream_reader.h"

#include <stdint.h>

#include "base/time/time.h"

namespace storage {

// Int64->double->int64_t conversions (e.g. through Blink) may lose some
// precision in the microsecond range. Allow 10us delta.
const int kModificationTimeAllowedDeltaMicroseconds = 10;

// Verify if the underlying file has not been modified.
bool FileStreamReader::VerifySnapshotTime(
    const base::Time& expected_modification_time,
    const base::File::Info& file_info) {
  return expected_modification_time.is_null() ||
         (expected_modification_time - file_info.last_modified)
                 .magnitude()
                 .InMicroseconds() < kModificationTimeAllowedDeltaMicroseconds;
}

}  // namespace storage
