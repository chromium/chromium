// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webdatabase/sqlite/sandboxed_vfs.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "sql/initialization.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sandboxed_vfs_file.h"
#include "third_party/blink/renderer/modules/webdatabase/web_database_host.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/sqlite/sqlite3.h"

#if OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace blink {

namespace {

// Name used to register the VFS with SQLite. Must be unique within Chrome.
constexpr char kSqliteVfsName[] = "renderer_sandboxed_vfs";

// Extracts the SandboxedVfs* stashed in a SQLite VFS structure.
SandboxedVfs* SandboxedVfsFromSqliteVfs(sqlite3_vfs* vfs) {
  DCHECK(vfs);
  return reinterpret_cast<SandboxedVfs*>(vfs->pAppData);
}

int SandboxedVfsOpen(sqlite3_vfs* vfs,
                     const char* full_path,
                     sqlite3_file* result_file,
                     int requested_flags,
                     int* granted_flags) {
  return SandboxedVfsFromSqliteVfs(vfs)->Open(full_path, result_file,
                                              requested_flags, granted_flags);
}
int SandboxedVfsDelete(sqlite3_vfs* vfs, const char* full_path, int sync_dir) {
  return SandboxedVfsFromSqliteVfs(vfs)->Delete(full_path, sync_dir);
}
int SandboxedVfsAccess(sqlite3_vfs* vfs,
                       const char* full_path,
                       int flags,
                       int* result) {
  return SandboxedVfsFromSqliteVfs(vfs)->Access(full_path, flags, result);
}
int SandboxedVfsFullPathname(sqlite3_vfs* vfs,
                             const char* file_path,
                             int result_size,
                             char* result) {
  return SandboxedVfsFromSqliteVfs(vfs)->FullPathname(file_path, result_size,
                                                      result);
}
int SandboxedVfsRandomness(sqlite3_vfs* vfs, int result_size, char* result) {
  return SandboxedVfsFromSqliteVfs(vfs)->Randomness(result_size, result);
}
int SandboxedVfsSleep(sqlite3_vfs* vfs, int microseconds) {
  return SandboxedVfsFromSqliteVfs(vfs)->Sleep(microseconds);
}
int SandboxedVfsGetLastError(sqlite3_vfs* vfs,
                             int message_size,
                             char* message) {
  return SandboxedVfsFromSqliteVfs(vfs)->GetLastError(message_size, message);
}
int SandboxedVfsCurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* result_ms) {
  return SandboxedVfsFromSqliteVfs(vfs)->CurrentTimeInt64(result_ms);
}

sqlite3_vfs SqliteVfsFor(SandboxedVfs* sandboxed_vfs) {
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
      kSqliteVfsName,
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

// Converts a SQLite full file path to a Blink string.
//
// The argument is guaranteed to be the result of a FullPathname() call, with
// an optional suffix. The suffix always starts with "-".
String StringFromFullPath(const char* full_path) {
  DCHECK(full_path);
  return String::FromUTF8(full_path);
}

// SQLite measures time according to the Julian calendar.
base::Time SqliteEpoch() {
  constexpr const double kMicroSecondsPerDay = 24 * 60 * 60 * 1000;
  // The ".5" is intentional -- days in the Julian calendar start at noon.
  // The offset is in the SQLite source code (os_unix.c) multiplied by 10.
  constexpr const double kUnixEpochAsJulianDay = 2440587.5;

  return base::Time::FromJsTime(-kUnixEpochAsJulianDay * kMicroSecondsPerDay);
}

}  // namespace

// static
SandboxedVfs& SandboxedVfs::GetInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(SandboxedVfs, instance, ());
  return instance;
}

SandboxedVfs::SandboxedVfs()
    : sandboxed_vfs_(SqliteVfsFor(this)),
      sqlite_epoch_(SqliteEpoch()),
      platform_(Platform::Current()),
      last_error_(base::File::FILE_OK) {
  sql::EnsureSqliteInitialized();

  // The register function returns a SQLite status as an int. The status is
  // ignored here. If registration fails, we'd want to report the error while
  // attempting to open a database. This is exactly what will happen, because
  // SQLite won't find the VFS we're asking for.
  sqlite3_vfs_register(&sandboxed_vfs_, /*make_default=*/0);
}

std::tuple<int, sqlite3*> SandboxedVfs::OpenDatabase(const String& filename) {
  sqlite3* connection;
  constexpr int open_flags =
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_PRIVATECACHE;
  int status = sqlite3_open_v2(filename.Utf8().c_str(), &connection, open_flags,
                               kSqliteVfsName);
  if (status != SQLITE_OK) {
    // SQLite creates a connection handle in most cases where open fails.
    if (connection) {
      sqlite3_close(connection);
      connection = nullptr;
    }
  }
  return {status, connection};
}

int SandboxedVfs::Open(const char* full_path,
                       sqlite3_file* result_file,
                       int requested_flags,
                       int* granted_flags) {
  DCHECK(full_path) << "WebSQL does not support creating temporary file names";
  DCHECK(result_file);
  DCHECK_EQ(0, requested_flags & SQLITE_OPEN_DELETEONCLOSE)
      << "SQLITE_OPEN_DELETEONCLOSE should not be used by WebSQL";
  DCHECK_EQ(0, requested_flags & SQLITE_OPEN_EXCLUSIVE)
      << "SQLITE_OPEN_EXCLUSIVE should not be used by WebSQL";

  String file_name = StringFromFullPath(full_path);

  // TODO(pwnall): This doesn't have to be synchronous. WebSQL's open sequence
  //               is asynchronous, so we could open all the needed files (DB,
  //               journal, etc.) asynchronously, and store them in a hash table
  //               that would be used here.
  base::File file =
      WebDatabaseHost::GetInstance().OpenFile(file_name, requested_flags);

  if (!file.IsValid()) {
    // TODO(pwnall): Figure out if we can remove the fallback to read-only.
    if (!(requested_flags & SQLITE_OPEN_READWRITE)) {
      // The SQLite API requires that pMethods is set to null even if the open
      // call returns a failure status.
      result_file->pMethods = nullptr;
      return SQLITE_CANTOPEN;
    }

    int new_flags =
        (requested_flags & ~SQLITE_OPEN_READWRITE) | SQLITE_OPEN_READONLY;
    return Open(full_path, result_file, new_flags, granted_flags);
  }

  SandboxedVfsFile::Create(std::move(file), std::move(file_name), this,
                           result_file);
  if (granted_flags)
    *granted_flags = requested_flags;
  return SQLITE_OK;
}

int SandboxedVfs::Delete(const char* full_path, int sync_dir) {
  DCHECK(full_path);
  return WebDatabaseHost::GetInstance().DeleteFile(
      StringFromFullPath(full_path), sync_dir);
}

int SandboxedVfs::Access(const char* full_path, int flags, int* result) {
  DCHECK(full_path);
  DCHECK(result);
  int32_t attributes = WebDatabaseHost::GetInstance().GetFileAttributes(
      StringFromFullPath(full_path));

  // TODO(pwnall): Make the mojo interface portable across OSes, instead of
  //               messing around with OS-dependent constants here.

#if defined(OS_WIN)
  const bool file_exists =
      static_cast<DWORD>(attributes) != INVALID_FILE_ATTRIBUTES;
#else
  const bool file_exists = attributes >= 0;
#endif  // defined(OS_WIN)

  if (!file_exists) {
    *result = 0;
    return SQLITE_OK;
  }

#if defined(OS_WIN)
  const bool can_read = true;
  const bool can_write = (attributes & FILE_ATTRIBUTE_READONLY) == 0;
#else
  const bool can_read = (attributes & R_OK) != 0;
  const bool can_write = (attributes & W_OK) != 0;
#endif  // defined(OS_WIN)

  switch (flags) {
    case SQLITE_ACCESS_EXISTS:
      *result = 1;
      break;
    case SQLITE_ACCESS_READ:
      *result = can_read ? 1 : 0;
      break;
    case SQLITE_ACCESS_READWRITE:
      *result = (can_read && can_write) ? 1 : 0;
      break;
    default:
      NOTREACHED() << "Unsupported xAccess flags: " << flags;
      return SQLITE_ERROR;
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
  base::PlatformThread::Sleep(base::TimeDelta::FromMicroseconds(microseconds));
  return SQLITE_OK;
}

int SandboxedVfs::GetLastError(int message_size, char* message) const {
  DCHECK_GE(message_size, 0);
  DCHECK(message_size == 0 || message);

  std::string error_string = base::File::ErrorToString(last_error_);
  size_t error_string_size = error_string.length() + 1;
  size_t copy_length =
      std::min(static_cast<size_t>(message_size), error_string_size);
  std::memcpy(message, error_string.c_str(), copy_length);
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

}  // namespace blink
