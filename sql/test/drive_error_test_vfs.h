// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_DRIVE_ERROR_TEST_VFS_H_
#define SQL_TEST_DRIVE_ERROR_TEST_VFS_H_

#include <vector>

#include "sql/sqlite_result_code.h"
#include "sql/test/test_vfs.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql::test {

// An SQLite VFS that can simulate drive errors.
class DriveErrorTestVfs : public TestVfs {
 public:
  DriveErrorTestVfs();
  ~DriveErrorTestVfs() override;

  int Write(sqlite3_file* file,
            const void* buffer,
            int size,
            sqlite3_int64 offset) override;
  void set_drive_full(bool drive_full) { drive_full_ = drive_full; }

  // Returns a vector of the errors produced by this `DriveErrorTestVfs`, in the
  // order they were produced. Duplicate errors are preserved, allowing the
  // callers to count the number of time each errors occurred. Does not include
  // errors produced by the underlying VFS.
  const std::vector<SqliteErrorCode>& errors_produced() const {
    return errors_produced_;
  }

 private:
  bool drive_full_ = false;

  std::vector<SqliteErrorCode> errors_produced_;
};

}  // namespace sql::test

#endif  // SQL_TEST_DRIVE_ERROR_TEST_VFS_H_
