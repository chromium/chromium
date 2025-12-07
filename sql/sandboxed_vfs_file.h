// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_SANDBOXED_VFS_FILE_H_
#define SQL_SANDBOXED_VFS_FILE_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

// SQLite VFS file interface of an vfs file that works in a sandboxed process.
class COMPONENT_EXPORT(SQL) SandboxedVfsFile {
 public:
  SandboxedVfsFile();
  virtual ~SandboxedVfsFile();

  // Bind the `vfs_file` to an instance in the given sqlite3_file buffer.
  static void BindSandboxedFile(SandboxedVfsFile* vfs_file,
                                sqlite3_file& buffer);

  // Extracts the instance bridged to the given SQLite VFS file.
  static SandboxedVfsFile& FromSqliteFile(sqlite3_file& sqlite_file);

  // sqlite3_file implementation.
  virtual int Close() = 0;
  virtual int Read(void* buffer, int size, sqlite3_int64 offset) = 0;
  virtual int Write(const void* buffer, int size, sqlite3_int64 offset) = 0;
  virtual int Truncate(sqlite3_int64 size) = 0;
  virtual int Sync(int flags) = 0;
  virtual int FileSize(sqlite3_int64* result_size) = 0;
  virtual int Lock(int mode) = 0;
  virtual int Unlock(int mode) = 0;
  virtual int CheckReservedLock(int* has_reserved_lock) = 0;
  virtual int FileControl(int opcode, void* data) = 0;
  virtual int SectorSize() = 0;
  virtual int DeviceCharacteristics() = 0;
  virtual int ShmMap(int page_index,
                     int page_size,
                     int extend_file_if_needed,
                     void volatile** result) = 0;
  virtual int ShmLock(int offset, int size, int flags) = 0;
  virtual void ShmBarrier() = 0;
  virtual int ShmUnmap(int also_delete_file) = 0;
  virtual int Fetch(sqlite3_int64 offset, int size, void** result) = 0;
  virtual int Unfetch(sqlite3_int64 offset, void* fetch_result) = 0;
};

// sqlite3_file "subclass" that bridges to a SandboxedVfsFile instance.
struct SandboxedVfsFileSqliteBridge {
  sqlite3_file sqlite_file;
  // `sandboxed_vfs_file` is not a raw_ptr<SandboxedVfsFile>, because
  // reinterpret_cast of uninitialized memory to raw_ptr can cause ref-counting
  // mismatch.
  RAW_PTR_EXCLUSION SandboxedVfsFile* sandboxed_vfs_file;

  static SandboxedVfsFileSqliteBridge& FromSqliteFile(
      sqlite3_file& sqlite_file);
};

}  // namespace sql

#endif  // SQL_SANDBOXED_VFS_FILE_H_
