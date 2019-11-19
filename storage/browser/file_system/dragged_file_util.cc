// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/dragged_file_util.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/native_file_util.h"

namespace storage {

using FileInfo = IsolatedContext::MountPointInfo;

namespace {

// Enumerate each path from a given fileinfo set.
// Used to enumerate top-level or recursive paths of an isolated filesystem.
class SetFileEnumerator : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  SetFileEnumerator(const std::vector<FileInfo>& files, bool recursive)
      : files_(files), recursive_(recursive) {
    file_iter_ = files_.begin();
  }
  ~SetFileEnumerator() override = default;

  // AbstractFileEnumerator overrides.
  base::FilePath Next() override {
    if (recursive_enumerator_) {
      base::FilePath platform_file = recursive_enumerator_->Next();
      if (platform_file.empty()) {
        recursive_enumerator_.reset();
      } else {
        file_info_.is_directory = recursive_enumerator_->IsDirectory();
        file_info_.size = recursive_enumerator_->Size();
        file_info_.last_modified = recursive_enumerator_->LastModifiedTime();
        return platform_file;
      }
    }
    if (file_iter_ != files_.end()) {
      base::FilePath platform_file = (file_iter_++)->path;
      NativeFileUtil::GetFileInfo(platform_file, &file_info_);
      if (recursive_ && file_info_.is_directory) {
        recursive_enumerator_ =
            NativeFileUtil::CreateFileEnumerator(platform_file, recursive_);
      }
      return platform_file;
    }
    return base::FilePath();
  }
  int64_t Size() override { return file_info_.size; }
  bool IsDirectory() override { return file_info_.is_directory; }
  base::Time LastModifiedTime() override { return file_info_.last_modified; }

 private:
  const std::vector<FileInfo> files_;
  const bool recursive_;
  std::vector<FileInfo>::const_iterator file_iter_;
  base::File::Info file_info_;
  std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator>
      recursive_enumerator_;
};

}  // namespace

//-------------------------------------------------------------------------

DraggedFileUtil::DraggedFileUtil() = default;

base::File::Error DraggedFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_path) {
  DCHECK(file_info);
  std::string filesystem_id;
  DCHECK(url.is_valid());
  if (url.path().empty()) {
    // The root directory case.
    // For now we leave three time fields (modified/accessed/creation time)
    // nullptr as it is not really clear what to be set for this virtual
    // directory.
    // TODO(kinuko): Maybe we want to set the time when this filesystem is
    // created (i.e. when the files/directories are dropped).
    file_info->is_directory = true;
    file_info->is_symbolic_link = false;
    file_info->size = 0;
    return base::File::FILE_OK;
  }
  base::File::Error error = NativeFileUtil::GetFileInfo(url.path(), file_info);
  if (IsHiddenItem(url.path()) && !base::FilePath().IsParent(url.path())) {
    // Don't follow symlinks unless it's the one that are selected by the user.
    return base::File::FILE_ERROR_NOT_FOUND;
  }
  if (error == base::File::FILE_OK)
    *platform_path = url.path();
  return error;
}

std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator>
DraggedFileUtil::CreateFileEnumerator(FileSystemOperationContext* context,
                                      const FileSystemURL& root,
                                      bool recursive) {
  DCHECK(root.is_valid());
  if (!root.path().empty())
    return LocalFileUtil::CreateFileEnumerator(context, root, recursive);

  // Root path case.
  std::vector<FileInfo> toplevels;
  IsolatedContext::GetInstance()->GetDraggedFileInfo(root.filesystem_id(),
                                                     &toplevels);
  return std::make_unique<SetFileEnumerator>(toplevels, recursive);
}

}  // namespace storage
