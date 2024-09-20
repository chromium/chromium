// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sandboxed_vfs.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "sql/initialization.h"
#include "sql/sandboxed_vfs_file.h"
#include "sql/vfs_wrapper.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

// Extracts the SandboxedVfs* stashed in a SQLite VFS structure.
SandboxedVfs& SandboxedVfsFromSqliteVfs(sqlite3_vfs& vfs) {
  return *reinterpret_cast<SandboxedVfs*>(vfs.pAppData);
}

int SandboxedVfsOpen(sqlite3_vfs* vfs,
                     const char* full_path,
                     sqlite3_file* result_file,
                     int requested_flags,
                     int* granted_flags) {
  return SandboxedVfsFromSqliteVfs(*vfs).Open(full_path, *result_file,
                                              requested_flags, granted_flags);
}
int SandboxedVfsDelete(sqlite3_vfs* vfs, const char* full_path, int sync_dir) {
  return SandboxedVfsFromSqliteVfs(*vfs).Delete(full_path, sync_dir);
}
int SandboxedVfsAccess(sqlite3_vfs* vfs,
                       const char* full_path,
                       int flags,
                       int* result) {
  return SandboxedVfsFromSqliteVfs(*vfs).Access(full_path, flags, *result);
}
int SandboxedVfsFullPathname(sqlite3_vfs* vfs,
                             const char* file_path,
                             int result_size,
                             char* result) {
  return SandboxedVfsFromSqliteVfs(*vfs).FullPathname(file_path, result_size,
                                                      result);
}
int SandboxedVfsRandomness(sqlite3_vfs* vfs, int result_size, char* result) {
  return SandboxedVfsFromSqliteVfs(*vfs).Randomness(result_size, result);
}
int SandboxedVfsSleep(sqlite3_vfs* vfs, int microseconds) {
  return SandboxedVfsFromSqliteVfs(*vfs).Sleep(microseconds);
}
int SandboxedVfsGetLastError(sqlite3_vfs* vfs,
                             int message_size,
                             char* message) {
  return SandboxedVfsFromSqliteVfs(*vfs).GetLastError(message_size, message);
}
int SandboxedVfsCurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* result_ms) {
  return SandboxedVfsFromSqliteVfs(*vfs).CurrentTimeInt64(result_ms);
}

sqlite3_vfs SqliteVfsFor(SandboxedVfs* sandboxed_vfs, const char* name) {
  DCHECK_EQ(sandboxed_vfs, reinterpret_cast<SandboxedVfs*>(
                               reinterpret_cast<void*>(sandboxed_vfs)))
      << "This implementation round-trips SandboxedVfs* via void*";

  // VFS API entry points are listed at https://www.sqlite.org/c3ref/vfs.html
  static constexpr int kSqliteVfsApiVersion = 3;

  // Maximum file path size.
  // TODO(pwnall): Obtain this from //base or some other good place.
  static constexpr int kSqliteMaxPathSize = 512;

  return {
      kSqliteVfsApiVersion,
      sizeof(SandboxedVfsFileSqliteBridge),
      kSqliteMaxPathSize,
      /*pNext=*/nullptr,
      name,
      /*pAppData=*/reinterpret_cast<void*>(sandboxed_vfs),
      SandboxedVfsOpen,
      SandboxedVfsDelete,
      SandboxedVfsAccess,
      SandboxedVfsFullPathname,
      /*xDlOpen=*/nullptr,
      /*xDlError=*/nullptr,
      /*xDlSym=*/nullptr,
      /*xDlClose=*/nullptr,
      SandboxedVfsRandomness,
      SandboxedVfsSleep,
      /*xCurrentTime=*/nullptr,  // Deprecated in API versions 2 and above.
      SandboxedVfsGetLastError,
      SandboxedVfsCurrentTimeInt64,
      /*xSetSystemCall=*/nullptr,
      /*xGetSystemCall=*/nullptr,
      /*xNextSystemCall=*/nullptr,
  };
}

// SQLite measures time according to the Julian calendar.
base::Time SqliteEpoch() {
  // The ".5" is intentional -- days in the Julian calendar start at noon.
  // The offset is in the SQLite source code (os_unix.c) multiplied by 10.
  constexpr const double kUnixEpochAsJulianDay = 2440587.5;

  return base::Time::FromMillisecondsSinceUnixEpoch(
      -kUnixEpochAsJulianDay * base::Time::kMillisecondsPerDay);
}

#if DCHECK_IS_ON()
// `full_path_cstr` must be a filename argument passed to the VFS from SQLite.
SandboxedVfsFileType VfsFileTypeFromPath(const char* full_path_cstr) {
  std::string_view full_path(full_path_cstr);

  if (full_path == sqlite3_filename_database(full_path_cstr)) {
    return SandboxedVfsFileType::kDatabase;
  }

  if (full_path == sqlite3_filename_journal(full_path_cstr)) {
    return SandboxedVfsFileType::kJournal;
  }

  if (full_path == sqlite3_filename_wal(full_path_cstr)) {
    return SandboxedVfsFileType::kWal;
  }

  NOTREACHED()
      << "Argument is not a file name buffer passed from SQLite to a VFS: "
      << full_path;
}
#endif  // DCHECK_IS_ON()

}  // namespace

// static
void SandboxedVfs::Register(const char* name,
                            std::unique_ptr<Delegate> delegate,
                            bool make_default) {
  static base::NoDestructor<std::vector<SandboxedVfs*>>
      registered_vfs_instances;
  sql::EnsureSqliteInitialized(/*create_wrapper=*/false);
  registered_vfs_instances->push_back(
      new SandboxedVfs(name, std::move(delegate), make_default));
}

int SandboxedVfs::Open(const char* full_path,
                       sqlite3_file& result_file,
                       int requested_flags,
                       int* granted_flags) {
  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(full_path);
  base::File file = delegate_->OpenFile(file_path, requested_flags);
  if (!file.IsValid()) {
    // TODO(pwnall): Figure out if we can remove the fallback to read-only.
    if (!(requested_flags & SQLITE_OPEN_READWRITE)) {
      // The SQLite API requires that pMethods is set to null even if the open
      // call returns a failure status.
      result_file.pMethods = nullptr;
      return SQLITE_CANTOPEN;
    }

    int new_flags =
        (requested_flags & ~SQLITE_OPEN_READWRITE) | SQLITE_OPEN_READONLY;
    return Open(full_path, result_file, new_flags, granted_flags);
  }

  SandboxedVfsFile::Create(std::move(file), std::move(file_path),
#if DCHECK_IS_ON()
                           VfsFileTypeFromPath(full_path),
#endif  // DCHECK_IS_ON()
                           this, result_file);
  if (granted_flags)
    *granted_flags = requested_flags;
  return SQLITE_OK;
}

int SandboxedVfs::Delete(const char* full_path, int sync_dir) {
  DCHECK(full_path);
  return delegate_->DeleteFile(base::FilePath::FromUTF8Unsafe(full_path),
                               sync_dir == 1);
}

int SandboxedVfs::Access(const char* full_path, int flags, int& result) {
  DCHECK(full_path);
  std::optional<PathAccessInfo> access =
      delegate_->GetPathAccess(base::FilePath::FromUTF8Unsafe(full_path));
  if (!access) {
    result = 0;
    return SQLITE_OK;
  }

  switch (flags) {
    case SQLITE_ACCESS_EXISTS:
      result = 1;
      break;
    case SQLITE_ACCESS_READ:
      result = access->can_read ? 1 : 0;
      break;
    case SQLITE_ACCESS_READWRITE:
      result = (access->can_read && access->can_write) ? 1 : 0;
      break;
    default:
      NOTREACHED() << "Unsupported xAccess flags: " << flags;
  }
  return SQLITE_OK;
}

int SandboxedVfs::FullPathname(const char* file_path,
                               int result_size,
                               char* result) {
  DCHECK(file_path);
  DCHECK_GT(result_size, 0);
  DCHECK(result);

  // Renderer processes cannot access files directly, so it doesn't make sense
  // to expose full paths here.
  size_t file_path_size = std::strlen(file_path) + 1;
  if (static_cast<size_t>(result_size) < file_path_size)
    return SQLITE_CANTOPEN;
  std::memcpy(result, file_path, file_path_size);
  return SQLITE_OK;
}

int SandboxedVfs::Randomness(int result_size, char* result) {
  DCHECK_GT(result_size, 0);
  DCHECK(result);

  // TODO(pwnall): Figure out if we need a real implementation.
  std::memset(result, 0, result_size);
  return result_size;
}

int SandboxedVfs::Sleep(int microseconds) {
  DCHECK_GE(microseconds, 0);
  base::PlatformThread::Sleep(base::Microseconds(microseconds));
  return SQLITE_OK;
}

int SandboxedVfs::GetLastError(int message_size, char* message) const {
  DCHECK_GE(message_size, 0);
  DCHECK(message_size == 0 || message);

  std::string error_string = base::File::ErrorToString(last_error_);
  size_t error_string_size = error_string.length() + 1;
  size_t copy_length =
      std::min(static_cast<size_t>(message_size), error_string_size);
  std::copy_n(error_string.c_str(), copy_length, message);
  // The return value is zero if the message fits in the buffer, and non-zero if
  // it does not fit.
  return copy_length != error_string_size;
}

int SandboxedVfs::CurrentTimeInt64(sqlite3_int64* result_ms) {
  DCHECK(result_ms);

  base::TimeDelta delta = base::Time::Now() - sqlite_epoch_;
  *result_ms = delta.InMilliseconds();
  return SQLITE_OK;
}

SandboxedVfs::SandboxedVfs(const char* name,
                           std::unique_ptr<Delegate> delegate,
                           bool make_default)
    : sandboxed_vfs_(SqliteVfsFor(this, name)),
      sqlite_epoch_(SqliteEpoch()),
      delegate_(std::move(delegate)),
      last_error_(base::File::FILE_OK) {
  if (make_default) {
    // This shouldn't override the VFS wrapper.
    DCHECK_NE(std::string_view(sqlite3_vfs_find(nullptr)->zName),
              kVfsWrapperName);
  }
  // The register function returns a SQLite status as an int. The status is
  // ignored here. If registration fails, we'd want to report the error while
  // attempting to open a database. This is exactly what will happen, because
  // SQLite won't find the VFS we're asking for.
  sqlite3_vfs_register(&sandboxed_vfs_, make_default ? 1 : 0);
}

}  // namespace sql
