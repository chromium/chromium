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

  int Open(sqlite3_vfs* vfs,
           const char* full_path,
           sqlite3_file* result_file,
           int requested_flags,
           int* granted_flags) override;
  int Delete(sqlite3_vfs* vfs, const char* full_path, int sync_dir) override;
  int Access(sqlite3_vfs* vfs,
             const char* full_path,
             int flags,
             int* result) override;

  int Close(sqlite3_file* file) override;
  int Read(sqlite3_file* file,
           void* buffer,
           int size,
           sqlite3_int64 offset) override;
  int Write(sqlite3_file* file,
            const void* buffer,
            int size,
            sqlite3_int64 offset) override;
  int Truncate(sqlite3_file* file, sqlite3_int64 size) override;
  int Sync(sqlite3_file* file, int flags) override;
  int FileSize(sqlite3_file* file, sqlite3_int64* result_size) override;
  int Lock(sqlite3_file* file, int mode) override;
  int Unlock(sqlite3_file* file, int mode) override;
  int CheckReservedLock(sqlite3_file* file, int* has_reserved_lock) override;
  int FileControl(sqlite3_file* file, int opcode, void* data) override;
  int ShmMap(sqlite3_file* file,
             int page_index,
             int page_size,
             int extend_file_if_needed,
             void volatile** result) override;
  int ShmLock(sqlite3_file* file, int offset, int size, int flags) override;
  int Fetch(sqlite3_file* file,
            sqlite3_int64 offset,
            int size,
            void** result) override;

  void set_drive_unusable(bool unusable) { drive_unusable_ = unusable; }
  void set_drive_full(bool drive_full) { drive_full_ = drive_full; }

  // Returns a vector of the errors produced by this `DriveErrorTestVfs`, in the
  // order they were produced. Duplicate errors are preserved, allowing the
  // callers to count the number of time each errors occurred. Does not include
  // errors produced by the underlying VFS.
  const std::vector<SqliteErrorCode>& errors_produced() const {
    return errors_produced_;
  }

 private:
  bool drive_unusable_ = false;
  bool drive_full_ = false;

  std::vector<SqliteErrorCode> errors_produced_;
};

}  // namespace sql::test

#endif  // SQL_TEST_DRIVE_ERROR_TEST_VFS_H_
