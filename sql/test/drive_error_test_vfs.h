// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_DRIVE_ERROR_TEST_VFS_H_
#define SQL_TEST_DRIVE_ERROR_TEST_VFS_H_

#include "sql/test/test_vfs.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql::test {

// An SQLite VFS that can simulate drive errors.
class DriveErrorTestVfs : public TestVfs {
 public:
  int Write(sqlite3_file* file,
            const void* buffer,
            int size,
            sqlite3_int64 offset) override;
  void set_drive_full(bool drive_full) { drive_full_ = drive_full; }

 private:
  bool drive_full_ = false;
};

}  // namespace sql::test

#endif  // SQL_TEST_DRIVE_ERROR_TEST_VFS_H_
