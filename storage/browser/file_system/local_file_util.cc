// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/local_file_util.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "storage/browser/file_system/async_file_util_adapter.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/native_file_util.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/gurl.h"

namespace storage {

AsyncFileUtil* AsyncFileUtil::CreateForLocalFileSystem() {
  return new AsyncFileUtilAdapter(std::make_unique<LocalFileUtil>());
}

class LocalFileUtil::LocalFileEnumerator
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  LocalFileEnumerator(const LocalFileUtil* file_util,
                      const base::FilePath& platform_root_path,
                      const base::FilePath& virtual_root_path,
                      bool recursive,
                      int file_type)
      : file_util_(file_util),
        file_enum_(platform_root_path,
                   recursive,
                   file_type,
                   base::FilePath::StringType(),
                   base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
                   base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION),
        platform_root_path_(platform_root_path),
        virtual_root_path_(virtual_root_path) {}

  ~LocalFileEnumerator() override = default;

  base::FilePath Next() override {
    while (true) {
      base::FilePath next = file_enum_.Next();
      if (next.empty()) {
        error_ = file_enum_.GetError();
        return next;
      } else if (file_util_->IsHiddenItem(next)) {
        continue;
      }
      file_util_info_ = file_enum_.GetInfo();

#if BUILDFLAG(IS_ANDROID)
      if (next.IsContentUri()) {
        return next;
      }
#endif

      base::FilePath path;
      platform_root_path_.AppendRelativePath(next, &path);
      return virtual_root_path_.Append(path);
    }
  }

  base::File::Error GetError() override { return error_; }

  base::FilePath GetName() override { return file_util_info_.GetName(); }

  int64_t Size() override { return file_util_info_.GetSize(); }

  base::Time LastModifiedTime() override {
    return file_util_info_.GetLastModifiedTime();
  }

  bool IsDirectory() override { return file_util_info_.IsDirectory(); }

 private:
  // The |LocalFileUtil| producing |this| is expected to remain valid
  // through the whole lifetime of the enumerator.
  const raw_ptr<const LocalFileUtil> file_util_;
  base::File::Error error_ = base::File::FILE_OK;
  base::FileEnumerator file_enum_;
  base::FileEnumerator::FileInfo file_util_info_;
  base::FilePath platform_root_path_;
  base::FilePath virtual_root_path_;
};

LocalFileUtil::LocalFileUtil() = default;

LocalFileUtil::~LocalFileUtil() = default;

base::File LocalFileUtil::CreateOrOpen(FileSystemOperationContext* context,
                                       const FileSystemURL& url,
                                       int file_flags) {
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return base::File(error);
  if (IsHiddenItem(file_path))
    return base::File(base::File::FILE_ERROR_NOT_FOUND);

  return NativeFileUtil::CreateOrOpen(file_path, file_flags);
}

base::File::Error LocalFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool* created) {
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  return NativeFileUtil::EnsureFileExists(file_path, created);
}

base::File::Error LocalFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive) {
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  return NativeFileUtil::CreateDirectory(file_path, exclusive, recursive);
}

base::File::Error LocalFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_file_path) {
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  if (IsHiddenItem(file_path))
    return base::File::FILE_ERROR_NOT_FOUND;

  error = NativeFileUtil::GetFileInfo(file_path, file_info);
  if (error == base::File::FILE_OK)
    *platform_file_path = file_path;
  return error;
}

std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator>
LocalFileUtil::CreateFileEnumerator(FileSystemOperationContext* context,
                                    const FileSystemURL& root_url,
                                    bool recursive) {
  base::FilePath file_path;
  if (GetLocalFilePath(context, root_url, &file_path) != base::File::FILE_OK) {
    return std::make_unique<EmptyFileEnumerator>();
  }
  return std::make_unique<LocalFileEnumerator>(
      this, file_path, root_url.path(), recursive,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
}

base::File::Error LocalFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::FilePath* local_file_path) {
  DCHECK(local_file_path);
  DCHECK(url.is_valid());
  if (url.path().empty()) {
    // Root direcory case, which should not be accessed.
    return base::File::FILE_ERROR_ACCESS_DENIED;
  }
  *local_file_path = url.path();
  return base::File::FILE_OK;
}

base::File::Error LocalFileUtil::Touch(FileSystemOperationContext* context,
                                       const FileSystemURL& url,
                                       const base::Time& last_access_time,
                                       const base::Time& last_modified_time) {
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  return NativeFileUtil::Touch(file_path, last_access_time, last_modified_time);
}

base::File::Error LocalFileUtil::Truncate(FileSystemOperationContext* context,
                                          const FileSystemURL& url,
                                          int64_t length) {
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  return NativeFileUtil::Truncate(file_path, length);
}

base::File::Error LocalFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    bool copy) {
  base::FilePath src_file_path;
  base::File::Error error = GetLocalFilePath(context, src_url, &src_file_path);
  if (error != base::File::FILE_OK)
    return error;

  base::FilePath dest_file_path;
  error = GetLocalFilePath(context, dest_url, &dest_file_path);
  if (error != base::File::FILE_OK)
    return error;

  return NativeFileUtil::CopyOrMoveFile(
      src_file_path, dest_file_path, options,
      NativeFileUtil::CopyOrMoveModeForDestination(dest_url, copy));
}

base::File::Error LocalFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url) {
  if (src_file_path.empty())
    return base::File::FILE_ERROR_INVALID_OPERATION;

  base::FilePath dest_file_path;
  base::File::Error error =
      GetLocalFilePath(context, dest_url, &dest_file_path);
  if (error != base::File::FILE_OK)
    return error;
  return NativeFileUtil::CopyOrMoveFile(
      src_file_path, dest_file_path, FileSystemOperation::CopyOrMoveOptionSet(),
      NativeFileUtil::CopyOrMoveModeForDestination(dest_url, true /* copy */));
}

base::File::Error LocalFileUtil::DeleteFile(FileSystemOperationContext* context,
                                            const FileSystemURL& url) {
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  return NativeFileUtil::DeleteFile(file_path);
}

base::File::Error LocalFileUtil::DeleteDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  return NativeFileUtil::DeleteDirectory(file_path);
}

ScopedFile LocalFileUtil::CreateSnapshotFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::File::Error* error,
    base::File::Info* file_info,
    base::FilePath* platform_path) {
  DCHECK(file_info);
  // We're just returning the local file information.
  *error = GetFileInfo(context, url, file_info, platform_path);
  if (*error == base::File::FILE_OK && file_info->is_directory)
    *error = base::File::FILE_ERROR_NOT_A_FILE;
  return ScopedFile();
}

bool LocalFileUtil::IsHiddenItem(const base::FilePath& local_file_path) const {
  // We should not follow symbolic links in sandboxed file system.
  return base::IsLink(local_file_path);
}

}  // namespace storage
