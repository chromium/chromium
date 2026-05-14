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

int DriveErrorTestVfs::Open(sqlite3_vfs* vfs,
                            const char* full_path,
                            sqlite3_file* result_file,
                            int requested_flags,
                            int* granted_flags) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIo);
    return SQLITE_IOERR;
  }
  return TestVfs::Open(vfs, full_path, result_file, requested_flags,
                       granted_flags);
}

int DriveErrorTestVfs::Delete(sqlite3_vfs* vfs,
                              const char* full_path,
                              int sync_dir) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoDelete);
    return SQLITE_IOERR_DELETE;
  }
  return TestVfs::Delete(vfs, full_path, sync_dir);
}

int DriveErrorTestVfs::Access(sqlite3_vfs* vfs,
                              const char* full_path,
                              int flags,
                              int* result) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoAccess);
    return SQLITE_IOERR_ACCESS;
  }
  return TestVfs::Access(vfs, full_path, flags, result);
}

int DriveErrorTestVfs::Close(sqlite3_file* file) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoClose);
    return SQLITE_IOERR_CLOSE;
  }
  return TestVfs::Close(file);
}

int DriveErrorTestVfs::Read(sqlite3_file* file,
                            void* buffer,
                            int size,
                            sqlite3_int64 offset) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoRead);
    return SQLITE_IOERR_READ;
  }
  return TestVfs::Read(file, buffer, size, offset);
}

int DriveErrorTestVfs::Truncate(sqlite3_file* file, sqlite3_int64 size) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoTruncate);
    return SQLITE_IOERR_TRUNCATE;
  }
  return TestVfs::Truncate(file, size);
}

int DriveErrorTestVfs::Sync(sqlite3_file* file, int flags) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoFsync);
    return SQLITE_IOERR_FSYNC;
  }
  return TestVfs::Sync(file, flags);
}

int DriveErrorTestVfs::FileSize(sqlite3_file* file,
                                sqlite3_int64* result_size) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoFstat);
    return SQLITE_IOERR_FSTAT;
  }
  return TestVfs::FileSize(file, result_size);
}

int DriveErrorTestVfs::Write(sqlite3_file* file,
                             const void* buffer,
                             int size,
                             sqlite3_int64 offset) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoWrite);
    return SQLITE_IOERR_WRITE;
  }
  if (drive_full_) {
    errors_produced_.push_back(SqliteErrorCode::kFullDisk);
    return SQLITE_FULL;
  }
  return TestVfs::Write(file, buffer, size, offset);
}

int DriveErrorTestVfs::Lock(sqlite3_file* file, int mode) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoLock);
    return SQLITE_IOERR_LOCK;
  }
  return TestVfs::Lock(file, mode);
}

int DriveErrorTestVfs::Unlock(sqlite3_file* file, int mode) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoUnlock);
    return SQLITE_IOERR_UNLOCK;
  }
  return TestVfs::Unlock(file, mode);
}

int DriveErrorTestVfs::CheckReservedLock(sqlite3_file* file,
                                         int* has_reserved_lock) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoCheckReservedLock);
    return SQLITE_IOERR_CHECKRESERVEDLOCK;
  }
  return TestVfs::CheckReservedLock(file, has_reserved_lock);
}

int DriveErrorTestVfs::FileControl(sqlite3_file* file, int opcode, void* data) {
  if (drive_unusable_) {
    switch (opcode) {
      case SQLITE_FCNTL_BEGIN_ATOMIC_WRITE: {
        errors_produced_.push_back(SqliteErrorCode::kIoBeginAtomic);
        return SQLITE_IOERR_BEGIN_ATOMIC;
      }
      case SQLITE_FCNTL_COMMIT_ATOMIC_WRITE: {
        errors_produced_.push_back(SqliteErrorCode::kIoCommitAtomic);
        return SQLITE_IOERR_COMMIT_ATOMIC;
      }
      case SQLITE_FCNTL_ROLLBACK_ATOMIC_WRITE: {
        errors_produced_.push_back(SqliteErrorCode::kIoRollbackAtomic);
        return SQLITE_IOERR_ROLLBACK_ATOMIC;
      }
      case SQLITE_FCNTL_SIZE_HINT:
      case SQLITE_FCNTL_MMAP_SIZE: {
        errors_produced_.push_back(SqliteErrorCode::kIoFstat);
        return SQLITE_IOERR_FSTAT;
      }
      case SQLITE_FCNTL_TEMPFILENAME: {
        errors_produced_.push_back(SqliteErrorCode::kIoGetTemporaryPath);
        return SQLITE_IOERR_GETTEMPPATH;
      }
      case SQLITE_FCNTL_SET_LOCKPROXYFILE:
      case SQLITE_FCNTL_GET_LOCKPROXYFILE:
      case SQLITE_FCNTL_EXTERNAL_READER: {
        errors_produced_.push_back(SqliteErrorCode::kIoLock);
        return SQLITE_IOERR_LOCK;
      }
    }
  }
  return TestVfs::FileControl(file, opcode, data);
}

int DriveErrorTestVfs::ShmMap(sqlite3_file* file,
                              int page_index,
                              int page_size,
                              int extend_file_if_needed,
                              void volatile** result) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoSharedMemoryMap);
    return SQLITE_IOERR_SHMMAP;
  }
  return TestVfs::ShmMap(file, page_index, page_size, extend_file_if_needed,
                         result);
}

int DriveErrorTestVfs::ShmLock(sqlite3_file* file,
                               int offset,
                               int size,
                               int flags) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoSharedMemoryLock);
    return SQLITE_IOERR_SHMLOCK;
  }
  return TestVfs::ShmLock(file, offset, size, flags);
}

int DriveErrorTestVfs::Fetch(sqlite3_file* file,
                             sqlite3_int64 offset,
                             int size,
                             void** result) {
  if (drive_unusable_) {
    errors_produced_.push_back(SqliteErrorCode::kIoFstat);
    return SQLITE_IOERR_FSTAT;
  }
  return TestVfs::Fetch(file, offset, size, result);
}

}  // namespace sql::test
