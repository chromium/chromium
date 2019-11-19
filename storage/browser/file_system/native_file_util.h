// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_NATIVE_FILE_UTIL_H_
#define STORAGE_BROWSER_FILE_SYSTEM_NATIVE_FILE_UTIL_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "storage/browser/file_system/file_system_file_util.h"

namespace base {
class Time;
}

namespace storage {

// A thin wrapper class for accessing the OS native filesystem.
// This performs common error checks necessary to implement FileUtil family
// in addition to perform native filesystem operations.
//
// For the error checks it performs please see the comment for
// FileSystemFileUtil interface
// (storage/browser/file_system/file_system_file_util.h).
//
// Note that all the methods of this class are static and this does NOT
// inherit from FileSystemFileUtil.
class COMPONENT_EXPORT(STORAGE_BROWSER) NativeFileUtil {
 public:
  enum CopyOrMoveMode { COPY_NOSYNC, COPY_SYNC, MOVE };

  static CopyOrMoveMode CopyOrMoveModeForDestination(
      const FileSystemURL& dest_url,
      bool copy);

  static base::File CreateOrOpen(const base::FilePath& path, int file_flags);
  static base::File::Error EnsureFileExists(const base::FilePath& path,
                                            bool* created);
  static base::File::Error CreateDirectory(const base::FilePath& path,
                                           bool exclusive,
                                           bool recursive);
  static base::File::Error GetFileInfo(const base::FilePath& path,
                                       base::File::Info* file_info);
  static std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator>
  CreateFileEnumerator(const base::FilePath& root_path, bool recursive);
  static base::File::Error Touch(const base::FilePath& path,
                                 const base::Time& last_access_time,
                                 const base::Time& last_modified_time);
  static base::File::Error Truncate(const base::FilePath& path, int64_t length);
  static bool PathExists(const base::FilePath& path);
  static bool DirectoryExists(const base::FilePath& path);
  static base::File::Error CopyOrMoveFile(
      const base::FilePath& src_path,
      const base::FilePath& dest_path,
      FileSystemOperation::CopyOrMoveOption option,
      CopyOrMoveMode mode);
  static base::File::Error DeleteFile(const base::FilePath& path);
  static base::File::Error DeleteDirectory(const base::FilePath& path);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NativeFileUtil);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_NATIVE_FILE_UTIL_H_
