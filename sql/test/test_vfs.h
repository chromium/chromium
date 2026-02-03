// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_TEST_VFS_H_
#define SQL_TEST_TEST_VFS_H_

#include "base/memory/raw_ptr.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql::test {

// Scope guard overriding SQLite's default Virtual File System (VFS). Unit tests
// can subclass `TestVfs` and override virtual methods to customize the file
// system's behavior. `TestVfs` automatically registers itself as default VFS on
// construction and restores the original VFS on destruction.
class TestVfs {
 public:
  // Name of this virtual file system.
  static constexpr char kName[] = "TestVfs";

  TestVfs();

  TestVfs(const TestVfs&) = delete;
  TestVfs& operator=(const TestVfs&) = delete;

  virtual ~TestVfs();

  // `sqlite3_vfs` implementation.
  virtual int Open(sqlite3_vfs* vfs,
                   const char* full_path,
                   sqlite3_file* result_file,
                   int requested_flags,
                   int* granted_flags);
  virtual int Delete(sqlite3_vfs* vfs, const char* full_path, int sync_dir);
  virtual int Access(sqlite3_vfs* vfs,
                     const char* full_path,
                     int flags,
                     int* result);
  virtual int FullPathname(sqlite3_vfs* vfs,
                           const char* file_path,
                           int result_size,
                           char* result);
  virtual int Randomness(sqlite3_vfs* vfs, int result_size, char* result);
  virtual int Sleep(sqlite3_vfs* vfs, int microseconds);
  virtual int GetLastError(sqlite3_vfs* vfs, int message_size, char* message);
  virtual int CurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* result_ms);

  // `sqlite3_file` implementation.
  virtual int Close(sqlite3_file* file);
  virtual int Read(sqlite3_file* file,
                   void* buffer,
                   int size,
                   sqlite3_int64 offset);
  virtual int Write(sqlite3_file* file,
                    const void* buffer,
                    int size,
                    sqlite3_int64 offset);
  virtual int Truncate(sqlite3_file* file, sqlite3_int64 size);
  virtual int Sync(sqlite3_file* file, int flags);
  virtual int FileSize(sqlite3_file* file, sqlite3_int64* result_size);
  virtual int Lock(sqlite3_file* file, int mode);
  virtual int Unlock(sqlite3_file* file, int mode);
  virtual int CheckReservedLock(sqlite3_file* file, int* has_reserved_lock);
  virtual int FileControl(sqlite3_file* file, int opcode, void* data);
  virtual int SectorSize(sqlite3_file* file);
  virtual int DeviceCharacteristics(sqlite3_file* file);
  virtual int ShmMap(sqlite3_file* file,
                     int page_index,
                     int page_size,
                     int extend_file_if_needed,
                     void volatile** result);
  virtual int ShmLock(sqlite3_file* file, int offset, int size, int flags);
  virtual void ShmBarrier(sqlite3_file* file);
  virtual int ShmUnmap(sqlite3_file* file, int also_delete_file);
  virtual int Fetch(sqlite3_file* file,
                    sqlite3_int64 offset,
                    int size,
                    void** result);
  virtual int Unfetch(sqlite3_file* file,
                      sqlite3_int64 offset,
                      void* fetch_result);

 private:
  // The SQLite3 definition of this `TestVfs`.
  sqlite3_vfs vfs_;

  // The original VFS that this `TestVfs` replaced.
  raw_ptr<sqlite3_vfs> original_vfs_ = nullptr;
};

}  // namespace sql::test

#endif  // SQL_TEST_TEST_VFS_H_
