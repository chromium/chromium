// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_SANDBOXED_VFS_FILE_H_
#define SQL_SANDBOXED_VFS_FILE_H_

#include "base/dcheck_is_on.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

// The file types associated with a SQLite database.
enum class SandboxedVfsFileType {
  // The main file, which stores the database pages.
  kDatabase,
  // The transaction rollback journal file. Used when WAL is off.
  // This file has the same path as the database, plus the "-journal" suffix.
  kJournal,
  // The Write-Ahead Log (WAL) file.
  // This file has the same path as the database, plus the "-wal" suffix.
  kWal,
};

class SandboxedVfs;

// SQLite VFS file implementation that works in a sandboxed process.
//
// An instance is created when SQLite calls into SandboxedVfs::Open(). The
// instance is deleted by a call to SandboxedVfsFile::Close().
//
// The SQLite VFS API includes a complex locking strategy documented in
// https://www.sqlite.org/lockingv3.html
//
// This implementation uses a simplified locking strategy, where we grab an
// exclusive lock when entering any of the modes that prepare for a transition
// to EXCLUSIVE. (These modes are RESERVED and PENDING). This approach is easy
// to implement on top of base::File's locking primitives, at the cost of some
// false contention, which makes us slower under high concurrency.
//
// SQLite's built-in VFSes use the OS support for locking a range of bytes in
// the file, rather locking than the whole file.
class SandboxedVfsFile {
 public:
  // Creates an instance in the given buffer. Note that `vfs` MUST outlive the
  // returned sqlite3_file object.
  static void Create(base::File file,
                     base::FilePath file_path,
#if DCHECK_IS_ON()
                     SandboxedVfsFileType file_type,
#endif  // DCHECK_IS_ON()
                     SandboxedVfs* vfs,
                     sqlite3_file& buffer);

  // Extracts the instance bridged to the given SQLite VFS file.
  static SandboxedVfsFile& FromSqliteFile(sqlite3_file& sqlite_file);

  // sqlite3_file implementation.
  int Close();
  int Read(void* buffer, int size, sqlite3_int64 offset);
  int Write(const void* buffer, int size, sqlite3_int64 offset);
  int Truncate(sqlite3_int64 size);
  int Sync(int flags);
  int FileSize(sqlite3_int64* result_size);
  int Lock(int mode);
  int Unlock(int mode);
  int CheckReservedLock(int* has_reserved_lock);
  int FileControl(int opcode, void* data);
  int SectorSize();
  int DeviceCharacteristics();
  int ShmMap(int page_index,
             int page_size,
             int extend_file_if_needed,
             void volatile** result);
  int ShmLock(int offset, int size, int flags);
  void ShmBarrier();
  int ShmUnmap(int also_delete_file);
  int Fetch(sqlite3_int64 offset, int size, void** result);
  int Unfetch(sqlite3_int64 offset, void* fetch_result);

 private:
  SandboxedVfsFile(base::File file,
                   base::FilePath file_path,
#if DCHECK_IS_ON()
                   SandboxedVfsFileType file_type,
#endif  // DCHECK_IS_ON()
                   SandboxedVfs* vfs);
  ~SandboxedVfsFile();

  // Constructed from a file handle passed from the browser process.
  base::File file_;
  // One of the SQLite locking mode constants.
  int sqlite_lock_mode_;
  // The SandboxedVfs that created this instance.
  const raw_ptr<SandboxedVfs> vfs_;
#if DCHECK_IS_ON()
  // Tracked to check assumptions about SQLite's locking protocol.
  const SandboxedVfsFileType file_type_;
#endif  // DCHECK_IS_ON()
  // Used to identify the file in IPCs to the browser process.
  const base::FilePath file_path_;
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
