// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/vfs_wrapper.h"

#include <cstring>
#include <functional>
#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/leak_annotations.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "third_party/sqlite/sqlite3.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/backup_util.h"
#include "base/files/file_util.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "sql/vfs_wrapper_fuchsia.h"
#endif

namespace sql {
namespace {

// https://www.sqlite.org/vfs.html - documents the overall VFS system.
//
// https://www.sqlite.org/c3ref/vfs.html - VFS methods.  This code tucks the
// wrapped VFS pointer into the wrapper's pAppData pointer.
//
// https://www.sqlite.org/c3ref/file.html - instance of an open file.  This code
// allocates a VfsFile for this, which contains a pointer to the wrapped file.
// Idiomatic SQLite would take the wrapped VFS szOsFile and increase it to store
// additional data as a prefix.

sqlite3_vfs* GetWrappedVfs(sqlite3_vfs* wrapped_vfs) {
  return static_cast<sqlite3_vfs*>(wrapped_vfs->pAppData);
}

VfsFile* AsVfsFile(sqlite3_file* wrapper_file) {
  return reinterpret_cast<VfsFile*>(wrapper_file);
}

sqlite3_file* GetWrappedFile(sqlite3_file* wrapper_file) {
  return AsVfsFile(wrapper_file)->wrapped_file;
}

int Close(sqlite3_file* sqlite_file) {
#if BUILDFLAG(IS_FUCHSIA)
  // Other platforms automatically unlock when the file descriptor is closed,
  // but the fuchsia virtual implementation doesn't have that so it needs an
  // explicit unlock on close.
  Unlock(sqlite_file, SQLITE_LOCK_NONE);
#endif

  VfsFile* file = AsVfsFile(sqlite_file);
  int r = file->wrapped_file->pMethods->xClose(file->wrapped_file);
  sqlite3_free(file->wrapped_file);

  // Memory will be freed with sqlite3_free(), so the destructor needs to be
  // called explicitly.
  file->~VfsFile();
  memset(file, '\0', sizeof(*file));
  return r;
}

int Read(sqlite3_file* sqlite_file, void* buf, int amt, sqlite3_int64 ofs)
{
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xRead(wrapped_file, buf, amt, ofs);
}

int Write(sqlite3_file* sqlite_file, const void* buf, int amt,
          sqlite3_int64 ofs)
{
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xWrite(wrapped_file, buf, amt, ofs);
}

int Truncate(sqlite3_file* sqlite_file, sqlite3_int64 size)
{
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xTruncate(wrapped_file, size);
}

int Sync(sqlite3_file* sqlite_file, int flags)
{
  SCOPED_UMA_HISTOGRAM_TIMER("Sql.vfs.SyncTime");
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xSync(wrapped_file, flags);
}

int FileSize(sqlite3_file* sqlite_file, sqlite3_int64* size)
{
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xFileSize(wrapped_file, size);
}

#if !BUILDFLAG(IS_FUCHSIA)

int Lock(sqlite3_file* sqlite_file, int file_lock)
{
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xLock(wrapped_file, file_lock);
}

int Unlock(sqlite3_file* sqlite_file, int file_lock)
{
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xUnlock(wrapped_file, file_lock);
}

int CheckReservedLock(sqlite3_file* sqlite_file, int* result)
{
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xCheckReservedLock(wrapped_file, result);
}

#endif  // !BUILDFLAG(IS_FUCHSIA)
// Else these functions are imported via vfs_wrapper_fuchsia.h.

int FileControl(sqlite3_file* sqlite_file, int op, void* arg)
{
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xFileControl(wrapped_file, op, arg);
}

int SectorSize(sqlite3_file* sqlite_file)
{
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xSectorSize(wrapped_file);
}

int DeviceCharacteristics(sqlite3_file* sqlite_file)
{
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xDeviceCharacteristics(wrapped_file);
}

int ShmMap(sqlite3_file *sqlite_file, int region, int size,
           int extend, void volatile **pp) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xShmMap(
      wrapped_file, region, size, extend, pp);
}

int ShmLock(sqlite3_file *sqlite_file, int ofst, int n, int flags) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xShmLock(wrapped_file, ofst, n, flags);
}

void ShmBarrier(sqlite3_file *sqlite_file) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  wrapped_file->pMethods->xShmBarrier(wrapped_file);
}

int ShmUnmap(sqlite3_file *sqlite_file, int del) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xShmUnmap(wrapped_file, del);
}

int Fetch(sqlite3_file *sqlite_file, sqlite3_int64 off, int amt, void **pp) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xFetch(wrapped_file, off, amt, pp);
}

int Unfetch(sqlite3_file *sqlite_file, sqlite3_int64 off, void *p) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xUnfetch(wrapped_file, off, p);
}

int Open(sqlite3_vfs* vfs, const char* file_name, sqlite3_file* wrapper_file,
         int desired_flags, int* used_flags) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);

  sqlite3_file* wrapped_file = static_cast<sqlite3_file*>(
      sqlite3_malloc(wrapped_vfs->szOsFile));
  if (!wrapped_file)
    return SQLITE_NOMEM;

  // NOTE(shess): SQLite's unixOpen() makes assumptions about the structure of
  // |file_name|.  Do not pass a local copy, here, only the passed-in value.
  int rc = wrapped_vfs->xOpen(wrapped_vfs,
                              file_name, wrapped_file,
                              desired_flags, used_flags);
  if (rc != SQLITE_OK) {
    sqlite3_free(wrapped_file);
    return rc;
  }
  // NOTE(shess): Any early exit from here needs to call xClose() on
  // |wrapped_file|.

#if BUILDFLAG(IS_APPLE)
  // When opening journal files, propagate backup exclusion from db.
  static int kJournalFlags =
      SQLITE_OPEN_MAIN_JOURNAL | SQLITE_OPEN_TEMP_JOURNAL |
      SQLITE_OPEN_SUBJOURNAL | SQLITE_OPEN_MASTER_JOURNAL;
  if (file_name && (desired_flags & kJournalFlags)) {
    // https://www.sqlite.org/c3ref/vfs.html indicates that the journal path
    // will have a suffix separated by "-" from the main database file name.
    std::string_view file_name_string_piece(file_name);
    size_t dash_index = file_name_string_piece.rfind('-');
    if (dash_index != std::string_view::npos) {
      base::FilePath database_file_path(
          std::string_view(file_name, dash_index));
      if (base::PathExists(database_file_path) &&
          base::apple::GetBackupExclusion(database_file_path)) {
        base::apple::SetBackupExclusion(base::FilePath(file_name_string_piece));
      }
    }
  }
#endif

  // |iVersion| determines what methods SQLite may call on the instance.
  // Having the methods which can't be proxied return an error may cause SQLite
  // to operate differently than if it didn't call those methods at all. To be
  // on the safe side, the wrapper sqlite3_io_methods version perfectly matches
  // the version of the wrapped files.
  //
  // At a first glance, it might be tempting to simplify the code by
  // restricting wrapping support to VFS version 3. However, this might fail on
  // Mac.
  //
  // On Mac, SQLite built with SQLITE_ENABLE_LOCKING_STYLE ends up using a VFS
  // that dynamically dispatches between a few variants of sqlite3_io_methods,
  // based on whether the opened database is on a local or on a remote (AFS,
  // NFS) filesystem. Some variants return a VFS version 1 structure.
  VfsFile* file = AsVfsFile(wrapper_file);

  // Call constructor explicitly since the memory is already allocated.
  new (file) VfsFile();

  file->wrapped_file = wrapped_file;

#if BUILDFLAG(IS_FUCHSIA)
  file->file_name = file_name;
#endif

  if (wrapped_file->pMethods->iVersion == 1) {
    static const sqlite3_io_methods io_methods = {
        1,
        Close,
        Read,
        Write,
        Truncate,
        Sync,
        FileSize,
        Lock,
        Unlock,
        CheckReservedLock,
        FileControl,
        SectorSize,
        DeviceCharacteristics,
    };
    file->methods = &io_methods;
  } else if (wrapped_file->pMethods->iVersion == 2) {
    static const sqlite3_io_methods io_methods = {
        2,
        Close,
        Read,
        Write,
        Truncate,
        Sync,
        FileSize,
        Lock,
        Unlock,
        CheckReservedLock,
        FileControl,
        SectorSize,
        DeviceCharacteristics,
        // Methods above are valid for version 1.
        ShmMap,
        ShmLock,
        ShmBarrier,
        ShmUnmap,
    };
    file->methods = &io_methods;
  } else {
    static const sqlite3_io_methods io_methods = {
        3,
        Close,
        Read,
        Write,
        Truncate,
        Sync,
        FileSize,
        Lock,
        Unlock,
        CheckReservedLock,
        FileControl,
        SectorSize,
        DeviceCharacteristics,
        // Methods above are valid for version 1.
        ShmMap,
        ShmLock,
        ShmBarrier,
        ShmUnmap,
        // Methods above are valid for version 2.
        Fetch,
        Unfetch,
    };
    file->methods = &io_methods;
  }
  return SQLITE_OK;
}

int Delete(sqlite3_vfs* vfs, const char* file_name, int sync_dir) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xDelete(wrapped_vfs, file_name, sync_dir);
}

int Access(sqlite3_vfs* vfs, const char* file_name, int flag, int* res) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xAccess(wrapped_vfs, file_name, flag, res);
}

int FullPathname(sqlite3_vfs* vfs, const char* relative_path,
                 int buf_size, char* absolute_path) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xFullPathname(
      wrapped_vfs, relative_path, buf_size, absolute_path);
}

int Randomness(sqlite3_vfs* vfs, int buf_size, char* buffer) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xRandomness(wrapped_vfs, buf_size, buffer);
}

int Sleep(sqlite3_vfs* vfs, int microseconds) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xSleep(wrapped_vfs, microseconds);
}

int GetLastError(sqlite3_vfs* vfs, int e, char* s) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xGetLastError(wrapped_vfs, e, s);
}

int CurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* now) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xCurrentTimeInt64(wrapped_vfs, now);
}

}  // namespace

void EnsureVfsWrapper() {
  if (sqlite3_vfs_find(kVfsWrapperName)) {
    return;
  }

  // Get the default VFS on all platforms except Fuchsia.
  static constexpr const char* kBaseVfsName =
#if BUILDFLAG(IS_FUCHSIA)
      "unix-none";
#else
      nullptr;
#endif
  sqlite3_vfs* wrapped_vfs = sqlite3_vfs_find(kBaseVfsName);
  CHECK(wrapped_vfs);

  // We only work with the VFS implementations listed below. If you're trying to
  // use this code with any other VFS, you're not in a good place.
  std::string_view vfs_name(wrapped_vfs->zName);
  CHECK(vfs_name == "unix" || vfs_name == "win32" || vfs_name == "unix-none" ||
        vfs_name == "storage_service")
      << "Wrapping unexpected VFS " << vfs_name;

  std::unique_ptr<sqlite3_vfs, std::function<void(sqlite3_vfs*)>> wrapper_vfs(
      static_cast<sqlite3_vfs*>(sqlite3_malloc(sizeof(sqlite3_vfs))),
      [](sqlite3_vfs* v) {
        sqlite3_free(v);
      });
  memset(wrapper_vfs.get(), '\0', sizeof(sqlite3_vfs));

  // VFS implementations should always work with a SQLite that only knows about
  // earlier versions.
  constexpr int kSqliteVfsApiVersion = 3;
  wrapper_vfs->iVersion = kSqliteVfsApiVersion;

  // All the SQLite VFS implementations used by Chrome should support the
  // version proxied here.
  DCHECK_GE(wrapped_vfs->iVersion, kSqliteVfsApiVersion);

  // Caller of xOpen() allocates this much space.
  wrapper_vfs->szOsFile = sizeof(VfsFile);

  wrapper_vfs->mxPathname = wrapped_vfs->mxPathname;
  wrapper_vfs->pNext = nullptr;  // Field used by SQLite.
  wrapper_vfs->zName = kVfsWrapperName;

  // Keep a reference to the wrapped vfs for use in methods.
  wrapper_vfs->pAppData = wrapped_vfs;

  // VFS methods.
  wrapper_vfs->xOpen = &Open;
  wrapper_vfs->xDelete = &Delete;
  wrapper_vfs->xAccess = &Access;
  wrapper_vfs->xFullPathname = &FullPathname;

  // SQLite's dynamic extension loading is disabled in Chrome. Not proxying
  // these methods lets us ship less logic and provides a tiny bit of extra
  // security, as we know for sure that SQLite will not dynamically load code.
  wrapper_vfs->xDlOpen = nullptr;
  wrapper_vfs->xDlError = nullptr;
  wrapper_vfs->xDlSym = nullptr;
  wrapper_vfs->xDlClose = nullptr;

  wrapper_vfs->xRandomness = &Randomness;
  wrapper_vfs->xSleep = &Sleep;

  // |xCurrentTime| is null when SQLite is built with SQLITE_OMIT_DEPRECATED, so
  // it does not need to be proxied.
  wrapper_vfs->xCurrentTime = nullptr;

  wrapper_vfs->xGetLastError = &GetLastError;

  // The methods above are in version 1 of SQLite's VFS API.

  DCHECK(wrapped_vfs->xCurrentTimeInt64 != nullptr);
  wrapper_vfs->xCurrentTimeInt64 = &CurrentTimeInt64;

  // The methods above are in version 2 of SQLite's VFS API.

  // The VFS system call interception API is intended for very low-level SQLite
  // testing and tweaks. Proxying these methods is not necessary because Chrome
  // does not do very low-level SQLite testing, and the VFS wrapper supports all
  // the needed tweaks.
  wrapper_vfs->xSetSystemCall = nullptr;
  wrapper_vfs->xGetSystemCall = nullptr;
  wrapper_vfs->xNextSystemCall = nullptr;

  // The methods above are in version 3 of sqlite_vfs.

  if (SQLITE_OK == sqlite3_vfs_register(wrapper_vfs.get(), /*makeDflt=*/1)) {
    ANNOTATE_LEAKING_OBJECT_PTR(wrapper_vfs.get());
    wrapper_vfs.release();
  }
}

}  // namespace sql
