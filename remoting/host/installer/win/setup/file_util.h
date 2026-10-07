// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_FILE_UTIL_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_FILE_UTIL_H_

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/win/windows_types.h"
#include "remoting/host/installer/win/setup/installer_constants.h"

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

// Deploys `file_names` from `source_dir` into `install_dir`. Creates
// `install_dir` if it does not exist and cleans up any unlocked "*.old" files
// from previous updates.
bool DeployPayloadFiles(
    const base::FilePath& source_dir,
    const base::FilePath& install_dir,
    base::span<const wchar_t* const> file_names = kPayloadFiles);

// Removes `file_names` and any leftover "*.old" files from `install_dir`, and
// removes `install_dir` if empty (or schedules it for reboot removal if locked
// files remain).
bool UninstallPayloadFiles(
    const base::FilePath& install_dir,
    base::span<const wchar_t* const> file_names = kPayloadFiles);

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_FILE_UTIL_H_
