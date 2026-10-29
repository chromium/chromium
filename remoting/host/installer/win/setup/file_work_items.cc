// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome Remote Desktop product-specific payload file `WorkItem`s for the
// Windows host installer.
//
// See `file_work_items.h` for the boundary between CRD payload file `WorkItem`s
// (which belong here) and generic file operation primitives (which belong in
// `file_util.{h,cc}`).

#include "remoting/host/installer/win/setup/file_work_items.h"

#include <windows.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "remoting/host/installer/win/setup/file_util.h"

namespace remoting::installer {

DeployFilesWorkItem::DeployFilesWorkItem(
    const base::FilePath& source_dir,
    const base::FilePath& install_dir,
    base::span<const wchar_t* const> file_names)
    : source_dir_(source_dir),
      install_dir_(install_dir),
      file_names_(file_names.begin(), file_names.end()) {}

DeployFilesWorkItem::~DeployFilesWorkItem() = default;

bool DeployFilesWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::CreateDirectory(install_dir_)) {
    PLOG(ERROR) << "Failed to create installation directory "
                << install_dir_.value();
    return false;
  }

  // Clean up any stale unlocked .old files from previous updates first.
  CleanupOldFiles(install_dir_);

  for (const auto& file_name : file_names_) {
    base::FilePath source_file = source_dir_.Append(file_name);
    base::FilePath dest_file = install_dir_.Append(file_name);

    if (!base::PathExists(source_file)) {
      LOG(ERROR) << "Source file does not exist: " << source_file.value();
      return false;
    }

    base::FilePath dest_parent = dest_file.DirName();
    if (!base::CreateDirectory(dest_parent)) {
      PLOG(ERROR) << "Failed to create directory: " << dest_parent.value();
      return false;
    }

    if (base::PathExists(dest_file)) {
      base::FilePath old_file;
      if (!MoveLockedFileToOldPath(dest_file, &old_file)) {
        LOG(ERROR) << "Failed to stage existing file: " << dest_file.value();
        return false;
      }
      renamed_files_.emplace_back(dest_file, old_file);
      ScheduleDeleteOnReboot(old_file);
    } else {
      created_files_.push_back(dest_file);
    }

    if (!CopyFileForSetup(source_file, dest_file)) {
      LOG(ERROR) << "Failed to copy " << source_file.value() << " to "
                 << dest_file.value();
      return false;
    }
  }

  return true;
}

void DeployFilesWorkItem::RollbackImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Delete newly installed files that did not previously exist in LIFO order.
  for (const auto& file : base::Reversed(created_files_)) {
    if (!DeleteFileForSetup(file)) {
      LOG(WARNING) << "Failed to delete newly created file during rollback: "
                   << file.value();
      base::FilePath temp_old_file;
      if (MoveLockedFileToOldPath(file, &temp_old_file)) {
        ScheduleDeleteOnReboot(temp_old_file);
      } else {
        ScheduleDeleteOnReboot(file);
      }
    }
  }

  // Restore previously existing files that were renamed to .old in LIFO order.
  for (const auto& [dest_file, old_file] : base::Reversed(renamed_files_)) {
    if (base::PathExists(dest_file) && !DeleteFileForSetup(dest_file)) {
      LOG(WARNING) << "Failed to delete staged file during rollback: "
                   << dest_file.value();
      base::FilePath temp_old_file;
      if (MoveLockedFileToOldPath(dest_file, &temp_old_file)) {
        ScheduleDeleteOnReboot(temp_old_file);
      } else {
        ScheduleDeleteOnReboot(dest_file);
      }
    }
    if (base::PathExists(old_file)) {
      PLOG_IF(ERROR,
              !MoveFileForSetup(old_file, dest_file, MOVEFILE_REPLACE_EXISTING))
          << "Failed to restore " << old_file.value() << " to "
          << dest_file.value() << " during rollback.";
    }
  }
}

DeleteFilesWorkItem::DeleteFilesWorkItem(
    const base::FilePath& install_dir,
    base::span<const wchar_t* const> file_names)
    : install_dir_(install_dir),
      file_names_(file_names.begin(), file_names.end()) {}

DeleteFilesWorkItem::~DeleteFilesWorkItem() = default;

bool DeleteFilesWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::DirectoryExists(install_dir_)) {
    return true;
  }

  CleanupOldFiles(install_dir_);

  bool all_deleted = true;
  for (const auto& file_name : file_names_) {
    base::FilePath file_path = install_dir_.Append(file_name);
    if (!base::PathExists(file_path)) {
      continue;
    }

    if (!DeleteFileForSetup(file_path)) {
      // If the file is still locked during uninstall, rename to .old and
      // schedule for deletion on reboot.
      base::FilePath old_file;
      if (MoveLockedFileToOldPath(file_path, &old_file)) {
        ScheduleDeleteOnReboot(old_file);
      } else {
        // If the file cannot be renamed to .old (e.g. opened without
        // FILE_SHARE_DELETE), schedule the original file for deletion on reboot
        // so the OS cleans it up and allows `install_dir_` to be removed on
        // reboot.
        ScheduleDeleteOnReboot(file_path);
        PLOG(WARNING) << "Failed to rename locked file " << file_path.value()
                      << " to .old path; scheduled for deletion on reboot.";
        all_deleted = false;
      }
    }
  }

  // Attempt non-recursive removal of `install_dir_` if it is now empty. If
  // locked `.old` files remain, schedule `install_dir_` for reboot removal
  // after the `.old` files.
  if (!::RemoveDirectoryW(install_dir_.value().c_str())) {
    DWORD error = ::GetLastError();
    if (error == ERROR_DIR_NOT_EMPTY) {
      ScheduleDeleteOnReboot(install_dir_);
    } else if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
      PLOG(WARNING) << "Failed to remove installation directory: "
                    << install_dir_.value();
    }
  }

  return all_deleted;
}

void DeleteFilesWorkItem::RollbackImpl() {
  // Best-effort uninstall rollback.
}

}  // namespace remoting::installer
