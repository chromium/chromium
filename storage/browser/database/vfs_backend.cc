// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/vfs_backend.h"

#include <stdint.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "third_party/sqlite/sqlite3.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace storage {

static const int kFileTypeMask = 0x00007F00;

// static
bool VfsBackend::OpenTypeIsReadWrite(int desired_flags) {
  return (desired_flags & SQLITE_OPEN_READWRITE) != 0;
}

// static
bool VfsBackend::OpenFileFlagsAreConsistent(int desired_flags) {
  const int file_type = desired_flags & kFileTypeMask;
  const bool is_exclusive = (desired_flags & SQLITE_OPEN_EXCLUSIVE) != 0;
  const bool is_delete = (desired_flags & SQLITE_OPEN_DELETEONCLOSE) != 0;
  const bool is_create = (desired_flags & SQLITE_OPEN_CREATE) != 0;
  const bool is_read_only = (desired_flags & SQLITE_OPEN_READONLY) != 0;
  const bool is_read_write = (desired_flags & SQLITE_OPEN_READWRITE) != 0;

  // All files should be opened either read-write or read-only, but not both.
  if (is_read_only == is_read_write)
    return false;

  // If a new file is created, it must also be writable.
  if (is_create && !is_read_write)
    return false;

  // If we're accessing an existing file, we cannot give exclusive access, and
  // we can't delete it.
  // Normally, we'd also check that 'is_delete' is false for a main DB, main
  // journal or master journal file; however, when in incognito mode, we use
  // the SQLITE_OPEN_DELETEONCLOSE flag when opening those files too and keep
  // an open handle to them for as long as the incognito profile is around.
  if ((is_exclusive || is_delete) && !is_create)
    return false;

  // Make sure we're opening the DB directory or that a file type is set.
  return (file_type == SQLITE_OPEN_MAIN_DB) ||
         (file_type == SQLITE_OPEN_TEMP_DB) ||
         (file_type == SQLITE_OPEN_MAIN_JOURNAL) ||
         (file_type == SQLITE_OPEN_TEMP_JOURNAL) ||
         (file_type == SQLITE_OPEN_SUBJOURNAL) ||
         (file_type == SQLITE_OPEN_MASTER_JOURNAL) ||
         (file_type == SQLITE_OPEN_TRANSIENT_DB);
}

// static
base::File VfsBackend::OpenFile(const base::FilePath& file_path,
                                int desired_flags) {
  DCHECK(!file_path.empty());

  // Verify the flags for consistency and create the database
  // directory if it doesn't exist.
  if (!OpenFileFlagsAreConsistent(desired_flags) ||
      !base::CreateDirectory(file_path.DirName())) {
    return base::File();
  }

  int flags = 0;
  flags |= base::File::FLAG_READ;
  if (desired_flags & SQLITE_OPEN_READWRITE)
    flags |= base::File::FLAG_WRITE;

  if (!(desired_flags & SQLITE_OPEN_MAIN_DB))
    flags |= base::File::FLAG_WIN_EXCLUSIVE_READ |
             base::File::FLAG_WIN_EXCLUSIVE_WRITE;

  if (desired_flags & SQLITE_OPEN_CREATE) {
    flags |= (desired_flags & SQLITE_OPEN_EXCLUSIVE)
                 ? base::File::FLAG_CREATE
                 : base::File::FLAG_OPEN_ALWAYS;
  } else {
    flags |= base::File::FLAG_OPEN;
  }

  if (desired_flags & SQLITE_OPEN_DELETEONCLOSE) {
    flags |= base::File::FLAG_WIN_TEMPORARY | base::File::FLAG_WIN_HIDDEN |
             base::File::FLAG_DELETE_ON_CLOSE;
  }

  // This flag will allow us to delete the file later on from the browser
  // process.
  flags |= base::File::FLAG_WIN_SHARE_DELETE;

  // This File may be passed to an untrusted process.
  flags = base::File::AddFlagsForPassingToUntrustedProcess(flags);

  // Try to open/create the DB file.
  return base::File(file_path, flags);
}

// static
base::File VfsBackend::OpenTempFileInDirectory(const base::FilePath& dir_path,
                                               int desired_flags) {
  // We should be able to delete temp files when they're closed
  // and create them as needed
  if (!(desired_flags & SQLITE_OPEN_DELETEONCLOSE) ||
      !(desired_flags & SQLITE_OPEN_CREATE)) {
    return base::File();
  }

  // Get a unique temp file name in the database directory.
  base::FilePath temp_file_path;
  if (!base::CreateTemporaryFileInDir(dir_path, &temp_file_path))
    return base::File();

  return OpenFile(temp_file_path, desired_flags);
}

// static
int VfsBackend::DeleteFile(const base::FilePath& file_path, bool sync_dir) {
  if (!base::PathExists(file_path))
    return SQLITE_OK;
  if (!base::DeleteFile(file_path))
    return SQLITE_IOERR_DELETE;

  int error_code = SQLITE_OK;
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  if (sync_dir) {
    base::File dir(file_path.DirName(), base::File::FLAG_READ);
    if (dir.IsValid()) {
      if (!dir.Flush())
        error_code = SQLITE_IOERR_DIR_FSYNC;
    } else {
      error_code = SQLITE_CANTOPEN;
    }
  }
#endif
  return error_code;
}

// static
uint32_t VfsBackend::GetFileAttributes(const base::FilePath& file_path) {
#if BUILDFLAG(IS_WIN)
  uint32_t attributes = ::GetFileAttributes(file_path.value().c_str());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  uint32_t attributes = 0;
  if (!access(file_path.value().c_str(), R_OK))
    attributes |= static_cast<uint32_t>(R_OK);
  if (!access(file_path.value().c_str(), W_OK))
    attributes |= static_cast<uint32_t>(W_OK);
  if (!attributes)
    attributes = -1;
#endif
  return attributes;
}

// static
bool VfsBackend::SetFileSize(const base::FilePath& file_path, int64_t size) {
  int flags = 0;
  flags |= base::File::FLAG_READ;
  flags |= base::File::FLAG_WRITE;
  flags |= base::File::FLAG_OPEN;
  base::File file = base::File(file_path, flags);
  if (!file.IsValid())
    return false;
  return file.SetLength(size);
}

}  // namespace storage
