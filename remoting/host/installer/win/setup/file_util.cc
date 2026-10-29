// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generic, product-agnostic file operation primitives and test seams for the
// Windows host installer.
//
// See `file_util.h` for the boundary between generic file primitives (which
// belong here) and Chrome Remote Desktop payload file `WorkItem`s (which belong
// in `file_work_items.{h,cc}`).

#include "remoting/host/installer/win/setup/file_util.h"

#include <windows.h>

#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "remoting/host/installer/win/setup/installer_constants.h"

namespace remoting::installer {

namespace {

CopyFileCallback& GetCopyFileCallback() {
  static base::NoDestructor<CopyFileCallback> callback(base::BindRepeating(
      [](const base::FilePath& from, const base::FilePath& to) {
        return base::CopyFile(from, to);
      }));
  return *callback;
}

DeleteFileCallback& GetDeleteFileCallback() {
  static base::NoDestructor<DeleteFileCallback> callback(base::BindRepeating(
      [](const base::FilePath& path) { return base::DeleteFile(path); }));
  return *callback;
}

MoveFileExCallback& GetMoveFileExCallback() {
  static base::NoDestructor<MoveFileExCallback> callback(base::BindRepeating(
      [](const wchar_t* existing, const wchar_t* new_name, DWORD flags) {
        return ::MoveFileExW(existing, new_name, flags);
      }));
  return *callback;
}

}  // namespace

bool CopyFileForSetup(const base::FilePath& from, const base::FilePath& to) {
  return GetCopyFileCallback().Run(from, to);
}

bool DeleteFileForSetup(const base::FilePath& path) {
  return GetDeleteFileCallback().Run(path);
}

bool MoveFileForSetup(const base::FilePath& from,
                      const base::FilePath& to,
                      DWORD flags) {
  return GetMoveFileExCallback().Run(from.value().c_str(),
                                     to.empty() ? nullptr : to.value().c_str(),
                                     flags) != FALSE;
}

void ScheduleDeleteOnReboot(const base::FilePath& path) {
  if (!GetMoveFileExCallback().Run(path.value().c_str(), nullptr,
                                   MOVEFILE_DELAY_UNTIL_REBOOT)) {
    PLOG(WARNING) << "Failed to schedule deletion on reboot for "
                  << path.value();
  }
}

bool MoveLockedFileToOldPath(const base::FilePath& dest_file,
                             base::FilePath* out_old_file) {
  constexpr int kMaxOldFileAttempts = 100;
  for (int i = 0; i < kMaxOldFileAttempts; ++i) {
    base::FilePath candidate =
        (i == 0) ? dest_file.AddExtension(kOldFileExtension)
                 : dest_file.AddExtension(base::NumberToWString(i))
                       .AddExtension(kOldFileExtension);
    if (base::PathExists(candidate) &&
        !GetDeleteFileCallback().Run(candidate)) {
      // `candidate` is still locked by an active process; try the next slot.
      continue;
    }

    // Because `candidate` was verified not to exist (or was just deleted),
    // any failure from MoveFileExW means `dest_file` itself cannot be renamed
    // (e.g. opened without FILE_SHARE_DELETE) and retrying other candidate
    // names will also fail.
    if (GetMoveFileExCallback().Run(dest_file.value().c_str(),
                                    candidate.value().c_str(),
                                    MOVEFILE_REPLACE_EXISTING)) {
      *out_old_file = candidate;
      return true;
    }

    PLOG(ERROR) << "Failed to rename locked file " << dest_file.value()
                << " to " << candidate.value();
    return false;
  }

  LOG(ERROR) << "Exhausted candidate .old paths for " << dest_file.value();
  return false;
}

void SetCopyFileCallbackForTesting(CopyFileCallback callback) {
  if (callback.is_null()) {
    GetCopyFileCallback() = base::BindRepeating(
        [](const base::FilePath& from, const base::FilePath& to) {
          return base::CopyFile(from, to);
        });
  } else {
    GetCopyFileCallback() = std::move(callback);
  }
}

void SetDeleteFileCallbackForTesting(DeleteFileCallback callback) {
  if (callback.is_null()) {
    GetDeleteFileCallback() = base::BindRepeating(
        [](const base::FilePath& path) { return base::DeleteFile(path); });
  } else {
    GetDeleteFileCallback() = std::move(callback);
  }
}

void SetMoveFileExCallbackForTesting(MoveFileExCallback callback) {
  if (callback.is_null()) {
    GetMoveFileExCallback() = base::BindRepeating(
        [](const wchar_t* existing, const wchar_t* new_name, DWORD flags) {
          return ::MoveFileExW(existing, new_name, flags);
        });
  } else {
    GetMoveFileExCallback() = std::move(callback);
  }
}

void CleanupOldFiles(const base::FilePath& install_dir) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::DirectoryExists(install_dir)) {
    return;
  }

  base::FileEnumerator enumerator(install_dir, /*recursive=*/false,
                                  base::FileEnumerator::FILES, L"*.old");
  for (base::FilePath old_file = enumerator.Next(); !old_file.empty();
       old_file = enumerator.Next()) {
    if (!GetDeleteFileCallback().Run(old_file)) {
      VLOG(1) << "Could not delete in-use old file: " << old_file.value();
    }
  }
}

bool InstallFileWithStagedRename(const base::FilePath& source_file,
                                 const base::FilePath& dest_file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::PathExists(source_file)) {
    LOG(ERROR) << "Source payload file does not exist: " << source_file.value();
    return false;
  }

  // First try a direct overwrite copy. This succeeds when `dest_file` does not
  // exist or is not locked by a running process.
  if (GetCopyFileCallback().Run(source_file, dest_file)) {
    return true;
  }

  if (!base::PathExists(dest_file)) {
    PLOG(ERROR) << "Failed to copy " << source_file.value() << " to "
                << dest_file.value();
    return false;
  }

  // `dest_file` is likely locked by an active host or desktop process. Rename
  // the running binary to "<dest_file>.old" (or "<dest_file>.<N>.old") so we
  // can copy the new file into the canonical path.
  base::FilePath old_file;
  if (!MoveLockedFileToOldPath(dest_file, &old_file)) {
    return false;
  }

  if (!GetCopyFileCallback().Run(source_file, dest_file)) {
    PLOG(ERROR) << "Failed to copy " << source_file.value() << " to "
                << dest_file.value() << " after renaming existing file.";
    // Best-effort rollback: move the original file back into place before
    // scheduling reboot deletion.
    GetMoveFileExCallback().Run(old_file.value().c_str(),
                                dest_file.value().c_str(),
                                MOVEFILE_REPLACE_EXISTING);
    return false;
  }

  // Schedule the renamed .old file for removal on the next reboot in case it
  // remains locked when the installer finishes.
  ScheduleDeleteOnReboot(old_file);
  return true;
}

}  // namespace remoting::installer
