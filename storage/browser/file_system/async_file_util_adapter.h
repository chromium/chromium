// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_ASYNC_FILE_UTIL_ADAPTER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_ASYNC_FILE_UTIL_ADAPTER_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "storage/browser/file_system/async_file_util.h"

namespace storage {

class FileSystemFileUtil;

// An adapter class for FileSystemFileUtil classes to provide asynchronous
// interface.
//
// A filesystem can do either:
// - implement a synchronous version of FileUtil by extending
//   FileSystemFileUtil and atach it to this adapter, or
// - directly implement AsyncFileUtil.
//
// This instance (as thus this->sync_file_util_) is guaranteed to be alive
// as far as FileSystemOperationContext given to each operation is kept alive.
class COMPONENT_EXPORT(STORAGE_BROWSER) AsyncFileUtilAdapter
    : public AsyncFileUtil {
 public:
  // Creates a new AsyncFileUtil for |sync_file_util|. This takes the
  // ownership of |sync_file_util|. (This doesn't take std::unique_ptr<> just
  // to save extra base::WrapUnique; in all use cases a new fresh FileUtil is
  // created only for this adapter.)
  explicit AsyncFileUtilAdapter(FileSystemFileUtil* sync_file_util);

  ~AsyncFileUtilAdapter() override;

  FileSystemFileUtil* sync_file_util() { return sync_file_util_.get(); }

  // AsyncFileUtil overrides.
  void CreateOrOpen(std::unique_ptr<FileSystemOperationContext> context,
                    const FileSystemURL& url,
                    int file_flags,
                    CreateOrOpenCallback callback) override;
  void EnsureFileExists(std::unique_ptr<FileSystemOperationContext> context,
                        const FileSystemURL& url,
                        EnsureFileExistsCallback callback) override;
  void CreateDirectory(std::unique_ptr<FileSystemOperationContext> context,
                       const FileSystemURL& url,
                       bool exclusive,
                       bool recursive,
                       StatusCallback callback) override;
  void GetFileInfo(std::unique_ptr<FileSystemOperationContext> context,
                   const FileSystemURL& url,
                   int fields,
                   GetFileInfoCallback callback) override;
  void ReadDirectory(std::unique_ptr<FileSystemOperationContext> context,
                     const FileSystemURL& url,
                     ReadDirectoryCallback callback) override;
  void Touch(std::unique_ptr<FileSystemOperationContext> context,
             const FileSystemURL& url,
             const base::Time& last_access_time,
             const base::Time& last_modified_time,
             StatusCallback callback) override;
  void Truncate(std::unique_ptr<FileSystemOperationContext> context,
                const FileSystemURL& url,
                int64_t length,
                StatusCallback callback) override;
  void CopyFileLocal(std::unique_ptr<FileSystemOperationContext> context,
                     const FileSystemURL& src_url,
                     const FileSystemURL& dest_url,
                     CopyOrMoveOption option,
                     CopyFileProgressCallback progress_callback,
                     StatusCallback callback) override;
  void MoveFileLocal(std::unique_ptr<FileSystemOperationContext> context,
                     const FileSystemURL& src_url,
                     const FileSystemURL& dest_url,
                     CopyOrMoveOption option,
                     StatusCallback callback) override;
  void CopyInForeignFile(std::unique_ptr<FileSystemOperationContext> context,
                         const base::FilePath& src_file_path,
                         const FileSystemURL& dest_url,
                         StatusCallback callback) override;
  void DeleteFile(std::unique_ptr<FileSystemOperationContext> context,
                  const FileSystemURL& url,
                  StatusCallback callback) override;
  void DeleteDirectory(std::unique_ptr<FileSystemOperationContext> context,
                       const FileSystemURL& url,
                       StatusCallback callback) override;
  void DeleteRecursively(std::unique_ptr<FileSystemOperationContext> context,
                         const FileSystemURL& url,
                         StatusCallback callback) override;
  void CreateSnapshotFile(std::unique_ptr<FileSystemOperationContext> context,
                          const FileSystemURL& url,
                          CreateSnapshotFileCallback callback) override;

 private:
  std::unique_ptr<FileSystemFileUtil> sync_file_util_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFileUtilAdapter);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_ASYNC_FILE_UTIL_ADAPTER_H_
