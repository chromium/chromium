// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webdatabase/sqlite/sandboxed_vfs_file.h"

#include <atomic>
#include <cstring>
#include <type_traits>
#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "sql/initialization.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sandboxed_vfs.h"
#include "third_party/blink/renderer/modules/webdatabase/web_database_host.h"
#include "third_party/sqlite/sqlite3.h"

namespace blink {

namespace {

int SandboxedClose(sqlite3_file* file) {
  return SandboxedVfsFile::FromSqliteFile(file)->Close();
}
int SandboxedRead(sqlite3_file* file,
                  void* buffer,
                  int size,
                  sqlite3_int64 offset) {
  return SandboxedVfsFile::FromSqliteFile(file)->Read(buffer, size, offset);
}
int SandboxedWrite(sqlite3_file* file,
                   const void* buffer,
                   int size,
                   sqlite3_int64 offset) {
  return SandboxedVfsFile::FromSqliteFile(file)->Write(buffer, size, offset);
}
int SandboxedTruncate(sqlite3_file* file, sqlite3_int64 size) {
  return SandboxedVfsFile::FromSqliteFile(file)->Truncate(size);
}
int SandboxedSync(sqlite3_file* file, int flags) {
  return SandboxedVfsFile::FromSqliteFile(file)->Sync(flags);
}
int SandboxedFileSize(sqlite3_file* file, sqlite3_int64* result_size) {
  return SandboxedVfsFile::FromSqliteFile(file)->FileSize(result_size);
}
int SandboxedLock(sqlite3_file* file, int mode) {
  return SandboxedVfsFile::FromSqliteFile(file)->Lock(mode);
}
int SandboxedUnlock(sqlite3_file* file, int mode) {
  return SandboxedVfsFile::FromSqliteFile(file)->Unlock(mode);
}
int SandboxedCheckReservedLock(sqlite3_file* file, int* has_reserved_lock) {
  return SandboxedVfsFile::FromSqliteFile(file)->CheckReservedLock(
      has_reserved_lock);
}
int SandboxedFileControl(sqlite3_file* file, int opcode, void* data) {
  return SandboxedVfsFile::FromSqliteFile(file)->FileControl(opcode, data);
}
int SandboxedSectorSize(sqlite3_file* file) {
  return SandboxedVfsFile::FromSqliteFile(file)->SectorSize();
}
int SandboxedDeviceCharacteristics(sqlite3_file* file) {
  return SandboxedVfsFile::FromSqliteFile(file)->DeviceCharacteristics();
}
int SandboxedShmMap(sqlite3_file* file,
                    int page_index,
                    int page_size,
                    int extend_file_if_needed,
                    void volatile** result) {
  return SandboxedVfsFile::FromSqliteFile(file)->ShmMap(
      page_index, page_size, extend_file_if_needed, result);
}
int SandboxedShmLock(sqlite3_file* file, int offset, int size, int flags) {
  return SandboxedVfsFile::FromSqliteFile(file)->ShmLock(offset, size, flags);
}
void SandboxedShmBarrier(sqlite3_file* file) {
  SandboxedVfsFile::FromSqliteFile(file)->ShmBarrier();
}
int SandboxedShmUnmap(sqlite3_file* file, int also_delete_file) {
  return SandboxedVfsFile::FromSqliteFile(file)->ShmUnmap(also_delete_file);
}
int SandboxedFetch(sqlite3_file* file,
                   sqlite3_int64 offset,
                   int size,
                   void** result) {
  return SandboxedVfsFile::FromSqliteFile(file)->Fetch(offset, size, result);
}
int SandboxedUnfetch(sqlite3_file* file,
                     sqlite3_int64 offset,
                     void* fetch_result) {
  return SandboxedVfsFile::FromSqliteFile(file)->Unfetch(offset, fetch_result);
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

// static
void SandboxedVfsFile::Create(base::File file,
                              String file_name,
                              SandboxedVfs* vfs,
                              sqlite3_file* buffer) {
  SandboxedVfsFileSqliteBridge* bridge =
      SandboxedVfsFileSqliteBridge::FromSqliteFile(buffer);
  bridge->blink_file =
      new SandboxedVfsFile(std::move(file), std::move(file_name), vfs);
  bridge->sqlite_file.pMethods = GetSqliteIoMethods();
}

// static
SandboxedVfsFile* SandboxedVfsFile::FromSqliteFile(sqlite3_file* sqlite_file) {
  return SandboxedVfsFileSqliteBridge::FromSqliteFile(sqlite_file)->blink_file;
}

int SandboxedVfsFile::Close() {
  file_.Close();
  delete this;
  return SQLITE_OK;
}

int SandboxedVfsFile::Read(void* buffer, int size, sqlite3_int64 offset) {
  DCHECK(buffer);
  DCHECK_GE(size, 0);
  DCHECK_GE(offset, 0);
  char* data = reinterpret_cast<char*>(buffer);

  // If we supported mmap()ed files, we'd check for a memory mapping here,
  // and try to fill as much of the request as possible from the mmap()ed
  // region.

  int bytes_read = file_.Read(offset, data, size);
  DCHECK_LE(bytes_read, size);
  if (bytes_read == size)
    return SQLITE_OK;

  // SQLite first reads the database header without locking the file. On
  // Windows, this read will fail if there is an exclusive lock on the file,
  // even if the current process owns that lock.
  if (sqlite_lock_mode_ == SQLITE_LOCK_NONE) {
    // The unlocked read is considered an optimization. SQLite can continue even
    // if the read fails, as long as failure is communicated by zeroing out the
    // output buffer.
    std::memset(data, 0, size);
    return SQLITE_OK;
  }

  if (bytes_read < 0) {
    vfs_->SetLastError(base::File::GetLastFileError());
    return SQLITE_IOERR_READ;
  }

  // SQLite requires that we fill the unread bytes in the buffer with zeros.
  std::memset(data + bytes_read, 0, size - bytes_read);
  return SQLITE_IOERR_SHORT_READ;
}

int SandboxedVfsFile::Write(const void* buffer,
                            int size,
                            sqlite3_int64 offset) {
  DCHECK(buffer);
  DCHECK_GE(size, 0);
  DCHECK_GE(offset, 0);
  const char* data = reinterpret_cast<const char*>(buffer);

  // If we supported mmap()ed files, we'd check for a memory mapping here,
  // and try to fill as much of the request as possible by copying to the
  // mmap()ed region.

  int bytes_written = file_.Write(offset, data, size);
  DCHECK_LE(bytes_written, size);
  if (bytes_written >= size)
    return SQLITE_OK;

  base::File::Error last_error = base::File::GetLastFileError();
  vfs_->SetLastError(last_error);
  if (last_error == base::File::Error::FILE_ERROR_NO_SPACE)
    return SQLITE_FULL;

  return SQLITE_IOERR_WRITE;
}

int SandboxedVfsFile::Truncate(sqlite3_int64 size) {
  if (file_.SetLength(size))
    return SQLITE_OK;

  // On Mac and Linux, the renderer sandbox blocks ftruncate(), so we have to
  // use a sync mojo IPC to ask the browser process to call ftruncate() for us.
  //
  // TODO(pwnall): Figure out if we can allow ftruncate() in the renderer. It
  //               would be useful for low-level storage APIs, like the upcoming
  //               filesystem API.
  if (!WebDatabaseHost::GetInstance().SetFileSize(file_name_, size))
    return SQLITE_IOERR_TRUNCATE;

  return SQLITE_OK;
}

int SandboxedVfsFile::Sync(int flags) {
  // NOTE: SQLite passes in (SQLITE_SYNC_NORMAL or SQLITE_SYNC_FULL),
  //       potentially OR-ed with SQLITE_SYNC_DATAONLY. Implementing these could
  //       lead to better performance.
  if (!file_.Flush()) {
    vfs_->SetLastError(base::File::GetLastFileError());
    return SQLITE_IOERR_FSYNC;
  }

  // The unix VFS also syncs the file's directory on the first xSync() call.
  // Chrome's LevelDB Env implementation does the same for specific files
  // (database manifests).
  //
  // For WebSQL, we would want to sync the directory at file open time, when the
  // file is opened for writing.

  return SQLITE_OK;
}

int SandboxedVfsFile::FileSize(sqlite3_int64* result_size) {
  int64_t length = file_.GetLength();
  if (length < 0) {
    vfs_->SetLastError(base::File::GetLastFileError());
    return SQLITE_IOERR_FSTAT;
  }

  // SQLite's unix VFS reports 1-byte files as empty. This is documented as a
  // workaround for a fairly obscure bug. See unixFileSize() in os_unix.c.
  if (length == 1)
    length = 0;

  *result_size = length;
  return SQLITE_OK;
}

namespace {

// True if our simplified implementation uses an exclusive lock for a mode.
bool IsExclusiveLockMode(int sqlite_lock_mode) {
  switch (sqlite_lock_mode) {
    case SQLITE_LOCK_NONE:
    case SQLITE_LOCK_SHARED:
      return false;

    case SQLITE_LOCK_RESERVED:
    case SQLITE_LOCK_PENDING:
    case SQLITE_LOCK_EXCLUSIVE:
      return true;
  }

  NOTREACHED() << "Unsupported mode: " << sqlite_lock_mode;
  return false;
}

}  // namespace

int SandboxedVfsFile::Lock(int mode) {
#if defined(OS_FUCHSIA)
  return SQLITE_IOERR_LOCK;
#else
  base::File::LockMode file_lock_mode = base::File::LockMode::kExclusive;

  switch (mode) {
    case SQLITE_LOCK_NONE:
      return SQLITE_OK;

    case SQLITE_LOCK_SHARED:
      if (sqlite_lock_mode_ != SQLITE_LOCK_NONE)
        return SQLITE_OK;

      file_lock_mode = base::File::LockMode::kShared;
      break;

    case SQLITE_LOCK_RESERVED:
      // A SHARED lock is required before a RESERVED lock is acquired.
      DCHECK_EQ(sqlite_lock_mode_, SQLITE_LOCK_SHARED);
      file_lock_mode = base::File::LockMode::kExclusive;
      break;

    case SQLITE_LOCK_PENDING:
      // A SHARED lock is required before a PENDING lock is acquired. The caller
      // may have a RESERVED lock.
      if (sqlite_lock_mode_ == SQLITE_LOCK_RESERVED) {
        sqlite_lock_mode_ = mode;
        return SQLITE_OK;
      }

      DCHECK_EQ(sqlite_lock_mode_, SQLITE_LOCK_SHARED);
      file_lock_mode = base::File::LockMode::kExclusive;
      break;

    case SQLITE_LOCK_EXCLUSIVE:
      if (IsExclusiveLockMode(sqlite_lock_mode_)) {
        sqlite_lock_mode_ = mode;
        return SQLITE_OK;
      }
      file_lock_mode = base::File::LockMode::kExclusive;
      break;

    default:
      NOTREACHED() << "Unimplemented xLock() mode: " << mode;
  }

  DCHECK_EQ(IsExclusiveLockMode(mode),
            file_lock_mode == base::File::LockMode::kExclusive)
      << "Incorrect file_lock_mode logic for SQLite mode: " << mode;

  // On POSIX, it would be possible to upgrade atomically from a shared lock to
  // an exclusive lock. This implementation prioritizes the simplicity of no
  // platform-specific code over being faster in high contention cases.
  if (sqlite_lock_mode_ != SQLITE_LOCK_NONE) {
    base::File::Error error = file_.Unlock();
    if (error != base::File::FILE_OK) {
      vfs_->SetLastError(base::File::GetLastFileError());
      return SQLITE_IOERR_LOCK;
    }
    sqlite_lock_mode_ = SQLITE_LOCK_NONE;
  }

  base::File::Error error = file_.Lock(file_lock_mode);
  if (error != base::File::FILE_OK) {
    vfs_->SetLastError(base::File::GetLastFileError());
    return SQLITE_IOERR_LOCK;
  }

  sqlite_lock_mode_ = mode;
  return SQLITE_OK;
#endif  // defined(OS_FUCHSIA)
}

int SandboxedVfsFile::Unlock(int mode) {
  // No-op if we're already unlocked or at the requested mode.
  if (sqlite_lock_mode_ == mode || sqlite_lock_mode_ == SQLITE_LOCK_NONE)
    return SQLITE_OK;

#if defined(OS_FUCHSIA)
  return SQLITE_IOERR_UNLOCK;
#else
  // On POSIX, it is possible to downgrade atomically from an exclusive lock to
  // a shared lock. SQLite's unix VFS takes advantage of this. This
  // implementation prioritizes the simplicity of no platform-specific code over
  // being faster in high contention cases.
  base::File::Error error = file_.Unlock();
  if (error != base::File::FILE_OK) {
    vfs_->SetLastError(base::File::GetLastFileError());
    return SQLITE_IOERR_UNLOCK;
  }

  if (mode == SQLITE_LOCK_NONE) {
    sqlite_lock_mode_ = mode;
    return SQLITE_OK;
  }

  DCHECK_EQ(mode, SQLITE_LOCK_SHARED);
  error = file_.Lock(base::File::LockMode::kShared);
  if (error == base::File::FILE_OK) {
    sqlite_lock_mode_ = mode;
    return SQLITE_OK;
  }

  // Gave up the exclusive lock, but failed to get a shared lock.
  vfs_->SetLastError(base::File::GetLastFileError());
  sqlite_lock_mode_ = SQLITE_LOCK_NONE;
  return SQLITE_IOERR_UNLOCK;
#endif  // defined(OS_FUCHSIA)
}

int SandboxedVfsFile::CheckReservedLock(int* has_reserved_lock) {
  if (IsExclusiveLockMode(sqlite_lock_mode_)) {
    *has_reserved_lock = 1;
    return SQLITE_OK;
  }

  if (sqlite_lock_mode_ == SQLITE_LOCK_SHARED) {
    // Lock modes at or above RESERVED map to exclusive locks in our simplified
    // implementation. If this process has a shared lock, no other process can
    // have an exclusive lock.
    *has_reserved_lock = 0;
    return SQLITE_OK;
  }

#if defined(OS_FUCHSIA)
  return SQLITE_IOERR_CHECKRESERVEDLOCK;
#else
  // On POSIX, it's possible to query the existing lock state of a file. The
  // SQLite unix VFS takes advantage of this. On Windows, this isn't the case.
  // Follow the strategy of the Windows VFS, which checks by trying to get an
  // exclusive lock on the file.
  base::File::Error error = file_.Lock(base::File::LockMode::kShared);
  if (error != base::File::FILE_OK) {
    *has_reserved_lock = 1;
    return SQLITE_OK;
  }

  *has_reserved_lock = 0;
  if (file_.Unlock() == base::File::FILE_OK)
    return SQLITE_OK;

  // We acquired a shared lock that we can't get rid of.
  sqlite_lock_mode_ = SQLITE_LOCK_SHARED;
  return SQLITE_IOERR_CHECKRESERVEDLOCK;
#endif  // defined(OS_FUCHSIA)
}

int SandboxedVfsFile::FileControl(int opcode, void* data) {
  switch (opcode) {
    case SQLITE_FCNTL_MMAP_SIZE:
      // Implementing memory-mapping will require handling this correctly.
      return SQLITE_NOTFOUND;
    default:
      return SQLITE_NOTFOUND;
  }
}

int SandboxedVfsFile::SectorSize() {
  return 0;
}

int SandboxedVfsFile::DeviceCharacteristics() {
  // TODO(pwnall): Figure out if we can get away with returning 0 on Windows.
#if defined(OS_WIN)
  return SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN;
#else
  // NOTE: SQLite's unix VFS attempts to detect the underlying filesystem and
  // sets some flags based on the result.
  return 0;
#endif  // OS_WIN
}

int SandboxedVfsFile::ShmMap(int page_index,
                             int page_size,
                             int extend_file_if_needed,
                             void volatile** result) {
  DCHECK_GE(page_index, 0);
  DCHECK_GE(page_size, 0);
  DCHECK(result);

  // NOTE: This would be needed by SQLite's WAL mode.
  *result = nullptr;
  return SQLITE_IOERR;
}

int SandboxedVfsFile::ShmLock(int offset, int size, int flags) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(size, 0);

  // NOTE: This would be needed by SQLite's WAL mode.
  return SQLITE_IOERR;
}

void SandboxedVfsFile::ShmBarrier() {
  // All writes to shared memory that have already been issued before this
  // function is called must complete before the function returns.
  std::atomic_thread_fence(std::memory_order_acq_rel);
}

int SandboxedVfsFile::ShmUnmap(int also_delete_file) {
  // NOTE: This would be needed by SQLite's WAL mode.
  return SQLITE_IOERR;
}

int SandboxedVfsFile::Fetch(sqlite3_int64 offset, int size, void** result) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(size, 0);
  DCHECK(result);

  // NOTE: This would be needed for mmap()ed file support.
  *result = nullptr;
  return SQLITE_IOERR;
}

int SandboxedVfsFile::Unfetch(sqlite3_int64 offset, void* fetch_result) {
  DCHECK_GE(offset, 0);
  DCHECK(fetch_result);

  // NOTE: This would be needed for mmap()ed file support.
  return SQLITE_IOERR;
}

SandboxedVfsFile::SandboxedVfsFile(base::File file,
                                   String file_name,
                                   SandboxedVfs* vfs)
    : file_(std::move(file)),
      sqlite_lock_mode_(SQLITE_LOCK_NONE),
      vfs_(vfs),
      file_name_(std::move(file_name)) {}

SandboxedVfsFile::~SandboxedVfsFile() = default;

// static
SandboxedVfsFileSqliteBridge* SandboxedVfsFileSqliteBridge::FromSqliteFile(
    sqlite3_file* sqlite_file) {
  static_assert(std::is_standard_layout<SandboxedVfsFileSqliteBridge>::value,
                "needed for the reinterpret_cast below");
  static_assert(offsetof(SandboxedVfsFileSqliteBridge, sqlite_file) == 0,
                "sqlite_file must be the first member of the struct.");

  SandboxedVfsFileSqliteBridge* bridge =
      reinterpret_cast<SandboxedVfsFileSqliteBridge*>(sqlite_file);
  DCHECK_EQ(sqlite_file, &bridge->sqlite_file)
      << "assumed by the reinterpret_casts in the implementation";
  return bridge;
}

}  // namespace blink
