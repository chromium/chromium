// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_LOCAL_FILE_UTIL_H_
#define STORAGE_BROWSER_FILE_SYSTEM_LOCAL_FILE_UTIL_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "storage/browser/file_system/file_system_file_util.h"

namespace base {
class Time;
}

namespace storage {

class FileSystemOperationContext;
class FileSystemURL;

// An instance of this class is created and owned by *FileSystemBackend.
class COMPONENT_EXPORT(STORAGE_BROWSER) LocalFileUtil
    : public FileSystemFileUtil {
 public:
  LocalFileUtil();

  LocalFileUtil(const LocalFileUtil&) = delete;
  LocalFileUtil& operator=(const LocalFileUtil&) = delete;

  ~LocalFileUtil() override;

  base::File CreateOrOpen(FileSystemOperationContext* context,
                          const FileSystemURL& url,
                          int file_flags) override;
  base::File::Error EnsureFileExists(FileSystemOperationContext* context,
                                     const FileSystemURL& url,
                                     bool* created) override;
  base::File::Error CreateDirectory(FileSystemOperationContext* context,
                                    const FileSystemURL& url,
                                    bool exclusive,
                                    bool recursive) override;
  base::File::Error GetFileInfo(FileSystemOperationContext* context,
                                const FileSystemURL& url,
                                base::File::Info* file_info,
                                base::FilePath* platform_file) override;

  // |this| must remain valid through the lifetime of the created enumerator.
  std::unique_ptr<AbstractFileEnumerator> CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url,
      bool recursive) override;

  base::File::Error GetLocalFilePath(FileSystemOperationContext* context,
                                     const FileSystemURL& file_system_url,
                                     base::FilePath* local_file_path) override;
  base::File::Error Touch(FileSystemOperationContext* context,
                          const FileSystemURL& url,
                          const base::Time& last_access_time,
                          const base::Time& last_modified_time) override;
  base::File::Error Truncate(FileSystemOperationContext* context,
                             const FileSystemURL& url,
                             int64_t length) override;
  base::File::Error CopyOrMoveFile(FileSystemOperationContext* context,
                                   const FileSystemURL& src_url,
                                   const FileSystemURL& dest_url,
                                   CopyOrMoveOptionSet options,
                                   bool copy) override;
  base::File::Error CopyInForeignFile(FileSystemOperationContext* context,
                                      const base::FilePath& src_file_path,
                                      const FileSystemURL& dest_url) override;
  base::File::Error DeleteFile(FileSystemOperationContext* context,
                               const FileSystemURL& url) override;
  base::File::Error DeleteDirectory(FileSystemOperationContext* context,
                                    const FileSystemURL& url) override;
  ScopedFile CreateSnapshotFile(FileSystemOperationContext* context,
                                const FileSystemURL& url,
                                base::File::Error* error,
                                base::File::Info* file_info,
                                base::FilePath* platform_path) override;

 protected:
  // Whether this item should not be accessed. For security reasons by default
  // symlinks are not exposed through |this|. If the derived implementation
  // can ensure safety of symlinks in some other way, it can lift this
  // restriction by overriding this method.
  virtual bool IsHiddenItem(const base::FilePath& local_file_path) const;

 private:
  class LocalFileEnumerator;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_LOCAL_FILE_UTIL_H_
