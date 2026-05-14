// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/drive_error_test_vfs.h"

#include <vector>

#include "sql/sqlite_result_code_values.h"
#include "sql/test/test_vfs.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql::test {

DriveErrorTestVfs::DriveErrorTestVfs() = default;
DriveErrorTestVfs::~DriveErrorTestVfs() = default;

int DriveErrorTestVfs::Write(sqlite3_file* file,
                             const void* buffer,
                             int size,
                             sqlite3_int64 offset) {
  if (drive_full_) {
    errors_produced_.push_back(SqliteErrorCode::kFullDisk);
    return SQLITE_FULL;
  }
  return TestVfs::Write(file, buffer, size, offset);
}

}  // namespace sql::test
