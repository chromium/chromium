// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_OBFUSCATED_FILE_UTIL_DELEGATE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_OBFUSCATED_FILE_UTIL_DELEGATE_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "storage/browser/file_system/native_file_util.h"

namespace storage {

// This delegate performs all ObfuscatedFileUtil tasks that actually touch disk.

class COMPONENT_EXPORT(STORAGE_BROWSER) ObfuscatedFileUtilDelegate {
 public:
  ObfuscatedFileUtilDelegate() = default;
  virtual ~ObfuscatedFileUtilDelegate() = default;

  virtual bool DirectoryExists(const base::FilePath& path) = 0;
  virtual size_t ComputeDirectorySize(const base::FilePath& path) = 0;
  virtual bool DeleteFileOrDirectory(const base::FilePath& path,
                                     bool recursive) = 0;
  virtual bool IsLink(const base::FilePath& file_path) = 0;
  virtual bool PathExists(const base::FilePath& path) = 0;

  virtual NativeFileUtil::CopyOrMoveMode CopyOrMoveModeForDestination(
      const FileSystemURL& dest_url,
      bool copy) = 0;
  virtual base::File CreateOrOpen(const base::FilePath& path,
                                  int file_flags) = 0;
  virtual base::File::Error EnsureFileExists(const base::FilePath& path,
                                             bool* created) = 0;
  virtual base::File::Error CreateDirectory(const base::FilePath& path,
                                            bool exclusive,
                                            bool recursive) = 0;
  virtual base::File::Error GetFileInfo(const base::FilePath& path,
                                        base::File::Info* file_info) = 0;
  virtual base::File::Error Touch(const base::FilePath& path,
                                  const base::Time& last_access_time,
                                  const base::Time& last_modified_time) = 0;
  virtual base::File::Error Truncate(const base::FilePath& path,
                                     int64_t length) = 0;
  virtual base::File::Error CopyOrMoveFile(
      const base::FilePath& src_path,
      const base::FilePath& dest_path,
      FileSystemOperation::CopyOrMoveOption option,
      NativeFileUtil::CopyOrMoveMode mode) = 0;
  virtual base::File::Error CopyInForeignFile(
      const base::FilePath& src_path,
      const base::FilePath& dest_path,
      FileSystemOperation::CopyOrMoveOption option,
      NativeFileUtil::CopyOrMoveMode mode) = 0;
  virtual base::File::Error DeleteFile(const base::FilePath& path) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ObfuscatedFileUtilDelegate);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_OBFUSCATED_FILE_UTIL_DELEGATE_H_
