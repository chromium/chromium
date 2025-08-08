// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sandboxed_vfs_file.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "sql/sandboxed_vfs.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

int SandboxedClose(sqlite3_file* file) {
  return SandboxedVfsFile::FromSqliteFile(*file).Close();
}
int SandboxedRead(sqlite3_file* file,
                  void* buffer,
                  int size,
                  sqlite3_int64 offset) {
  return SandboxedVfsFile::FromSqliteFile(*file).Read(buffer, size, offset);
}
int SandboxedWrite(sqlite3_file* file,
                   const void* buffer,
                   int size,
                   sqlite3_int64 offset) {
  return SandboxedVfsFile::FromSqliteFile(*file).Write(buffer, size, offset);
}
int SandboxedTruncate(sqlite3_file* file, sqlite3_int64 size) {
  return SandboxedVfsFile::FromSqliteFile(*file).Truncate(size);
}
int SandboxedSync(sqlite3_file* file, int flags) {
  return SandboxedVfsFile::FromSqliteFile(*file).Sync(flags);
}
int SandboxedFileSize(sqlite3_file* file, sqlite3_int64* result_size) {
  return SandboxedVfsFile::FromSqliteFile(*file).FileSize(result_size);
}
int SandboxedLock(sqlite3_file* file, int mode) {
  return SandboxedVfsFile::FromSqliteFile(*file).Lock(mode);
}
int SandboxedUnlock(sqlite3_file* file, int mode) {
  return SandboxedVfsFile::FromSqliteFile(*file).Unlock(mode);
}
int SandboxedCheckReservedLock(sqlite3_file* file, int* has_reserved_lock) {
  return SandboxedVfsFile::FromSqliteFile(*file).CheckReservedLock(
      has_reserved_lock);
}
int SandboxedFileControl(sqlite3_file* file, int opcode, void* data) {
  return SandboxedVfsFile::FromSqliteFile(*file).FileControl(opcode, data);
}
int SandboxedSectorSize(sqlite3_file* file) {
  return SandboxedVfsFile::FromSqliteFile(*file).SectorSize();
}
int SandboxedDeviceCharacteristics(sqlite3_file* file) {
  return SandboxedVfsFile::FromSqliteFile(*file).DeviceCharacteristics();
}
int SandboxedShmMap(sqlite3_file* file,
                    int page_index,
                    int page_size,
                    int extend_file_if_needed,
                    void volatile** result) {
  return SandboxedVfsFile::FromSqliteFile(*file).ShmMap(
      page_index, page_size, extend_file_if_needed, result);
}
int SandboxedShmLock(sqlite3_file* file, int offset, int size, int flags) {
  return SandboxedVfsFile::FromSqliteFile(*file).ShmLock(offset, size, flags);
}
void SandboxedShmBarrier(sqlite3_file* file) {
  SandboxedVfsFile::FromSqliteFile(*file).ShmBarrier();
}
int SandboxedShmUnmap(sqlite3_file* file, int also_delete_file) {
  return SandboxedVfsFile::FromSqliteFile(*file).ShmUnmap(also_delete_file);
}
int SandboxedFetch(sqlite3_file* file,
                   sqlite3_int64 offset,
                   int size,
                   void** result) {
  return SandboxedVfsFile::FromSqliteFile(*file).Fetch(offset, size, result);
}
int SandboxedUnfetch(sqlite3_file* file,
                     sqlite3_int64 offset,
                     void* fetch_result) {
  return SandboxedVfsFile::FromSqliteFile(*file).Unfetch(offset, fetch_result);
}

const sqlite3_io_methods* GetSqliteIoMethods() {
  // VFS IO API entry points are listed at
  // https://www.sqlite.org/c3ref/io_methods.html
  static constexpr int kSqliteVfsIoApiVersion = 3;

  static const sqlite3_io_methods kIoMethods = {
      kSqliteVfsIoApiVersion,
      SandboxedClose,
      SandboxedRead,
      SandboxedWrite,
      SandboxedTruncate,
      SandboxedSync,
      SandboxedFileSize,
      SandboxedLock,
      SandboxedUnlock,
      SandboxedCheckReservedLock,
      SandboxedFileControl,
      SandboxedSectorSize,
      SandboxedDeviceCharacteristics,
      SandboxedShmMap,
      SandboxedShmLock,
      SandboxedShmBarrier,
      SandboxedShmUnmap,
      SandboxedFetch,
      SandboxedUnfetch,
  };

  return &kIoMethods;
}

}  // namespace

SandboxedVfsFile::SandboxedVfsFile() = default;
SandboxedVfsFile::~SandboxedVfsFile() = default;

// static
void SandboxedVfsFile::BindSandboxedFile(SandboxedVfsFile* vfs_file,
                                         sqlite3_file& buffer) {
  SandboxedVfsFileSqliteBridge& bridge =
      SandboxedVfsFileSqliteBridge::FromSqliteFile(buffer);
  bridge.sandboxed_vfs_file = vfs_file;
  bridge.sqlite_file.pMethods = GetSqliteIoMethods();
}

// static
SandboxedVfsFile& SandboxedVfsFile::FromSqliteFile(sqlite3_file& sqlite_file) {
  return *SandboxedVfsFileSqliteBridge::FromSqliteFile(sqlite_file)
              .sandboxed_vfs_file;
}

// static
SandboxedVfsFileSqliteBridge& SandboxedVfsFileSqliteBridge::FromSqliteFile(
    sqlite3_file& sqlite_file) {
  static_assert(std::is_standard_layout<SandboxedVfsFileSqliteBridge>::value,
                "needed for the reinterpret_cast below");
  static_assert(offsetof(SandboxedVfsFileSqliteBridge, sqlite_file) == 0,
                "sqlite_file must be the first member of the struct.");

  SandboxedVfsFileSqliteBridge& bridge =
      reinterpret_cast<SandboxedVfsFileSqliteBridge&>(sqlite_file);
  DCHECK_EQ(&sqlite_file, &bridge.sqlite_file)
      << "assumed by the reinterpret_casts in the implementation";
  return bridge;
}

}  // namespace sql
