// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_FILE_WORK_ITEMS_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_FILE_WORK_ITEMS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "chrome/installer/util/work_item.h"
#include "remoting/host/installer/win/setup/installer_constants.h"

namespace remoting::installer {

// A WorkItem that deploys payload files from `source_dir` into `install_dir`.
// If destination files are in use, they are staged with staged renaming (.old)
// and scheduled for deletion on reboot. On rollback, newly installed files are
// deleted and previous in-use files are restored from .old.
class DeployFilesWorkItem : public WorkItem {
 public:
  DeployFilesWorkItem(
      const base::FilePath& source_dir,
      const base::FilePath& install_dir,
      base::span<const wchar_t* const> file_names = kPayloadFiles);
  DeployFilesWorkItem(const DeployFilesWorkItem&) = delete;
  DeployFilesWorkItem& operator=(const DeployFilesWorkItem&) = delete;
  ~DeployFilesWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  base::FilePath source_dir_;
  base::FilePath install_dir_;
  std::vector<std::wstring> file_names_;

  std::vector<base::FilePath> created_files_;
  std::vector<std::pair<base::FilePath, base::FilePath>> renamed_files_;
};

// A WorkItem that deletes all installed Chrome Remote Desktop files.
class DeleteFilesWorkItem : public WorkItem {
 public:
  explicit DeleteFilesWorkItem(
      const base::FilePath& install_dir,
      base::span<const wchar_t* const> file_names = kPayloadFiles);
  DeleteFilesWorkItem(const DeleteFilesWorkItem&) = delete;
  DeleteFilesWorkItem& operator=(const DeleteFilesWorkItem&) = delete;
  ~DeleteFilesWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  base::FilePath install_dir_;
  std::vector<std::wstring> file_names_;
};

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_FILE_WORK_ITEMS_H_
