// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_FILE_UTIL_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_FILE_UTIL_H_

// Generic, product-agnostic file operation primitives and test seams for the
// Windows host installer.
//
// What belongs here:
// - Low-level file operation wrappers (`CopyFileForSetup`,
//   `DeleteFileForSetup`, `MoveFileForSetup`) and test injection callbacks.
// - Generic locked-file staging helpers (`MoveLockedFileToOldPath`,
//   `ScheduleDeleteOnReboot`, `CleanupOldFiles`,
//   `InstallFileWithStagedRename`).
//
// What does NOT belong here:
// - Chrome Remote Desktop product-specific payload file lists (`kPayloadFiles`)
//   or `WorkItem` subclasses for deploying/uninstalling CRD payload files. Put
//   those in `file_work_items.{h,cc}`.

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/win/windows_types.h"

namespace remoting::installer {

// Function signatures for file operations intercepted during tests.
using CopyFileCallback =
    base::RepeatingCallback<bool(const base::FilePath&, const base::FilePath&)>;
using DeleteFileCallback = base::RepeatingCallback<bool(const base::FilePath&)>;
using MoveFileExCallback =
    base::RepeatingCallback<BOOL(const wchar_t*, const wchar_t*, DWORD)>;

// Sets callbacks invoked instead of base::CopyFile, base::DeleteFile, and
// ::MoveFileExW during unit tests to simulate file locks and failure modes
// deterministically without relying on OS-dependent kernel file mapping
// behavior. Passing null callbacks restores the default system functions.
void SetCopyFileCallbackForTesting(CopyFileCallback callback);
void SetDeleteFileCallbackForTesting(DeleteFileCallback callback);
void SetMoveFileExCallbackForTesting(MoveFileExCallback callback);

// Wrappers that invoke the mock callbacks if set, or fall back to system APIs.
bool CopyFileForSetup(const base::FilePath& from, const base::FilePath& to);
bool DeleteFileForSetup(const base::FilePath& path);
bool MoveFileForSetup(const base::FilePath& from,
                      const base::FilePath& to,
                      DWORD flags);

// Schedules `path` for deletion on the next reboot using
// MOVEFILE_DELAY_UNTIL_REBOOT via MoveFileExW.
void ScheduleDeleteOnReboot(const base::FilePath& path);

// Finds an available "<dest_file>.old" (or "<dest_file>.<N>.old") path and
// moves `dest_file` to it. Supports multiple updates while earlier ".old"
// binaries remain locked by long-running sessions.
bool MoveLockedFileToOldPath(const base::FilePath& dest_file,
                             base::FilePath* out_old_file);

// Deletes any stale "*.old" files in `install_dir` that are no longer locked
// by a running process.
void CleanupOldFiles(const base::FilePath& install_dir);

// Installs `source_file` to `dest_file`. If `dest_file` already exists and is
// locked by a running process, `dest_file` is renamed to "<dest_file>.old"
// (or "<dest_file>.<N>.old" if previous .old files are still locked) and
// scheduled for deletion on reboot before copying `source_file` into
// `dest_file`.
bool InstallFileWithStagedRename(const base::FilePath& source_file,
                                 const base::FilePath& dest_file);

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_FILE_UTIL_H_
