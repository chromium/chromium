// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_BACKEND_H_
#define STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_BACKEND_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "storage/browser/file_system/async_file_util_adapter.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {
class AsyncFileUtilAdapter;
class FileSystemQuotaUtil;
}

namespace storage {

// This should be only used for testing.
// This file system backend uses LocalFileUtil and stores data file
// under the given directory.
class TestFileSystemBackend : public FileSystemBackend {
 public:
  TestFileSystemBackend(base::SequencedTaskRunner* task_runner,
                        const base::FilePath& base_path);

  TestFileSystemBackend(const TestFileSystemBackend&) = delete;
  TestFileSystemBackend& operator=(const TestFileSystemBackend&) = delete;

  ~TestFileSystemBackend() override;

  // FileSystemBackend implementation.
  bool CanHandleType(FileSystemType type) const override;
  void Initialize(FileSystemContext* context) override;
  void ResolveURL(const FileSystemURL& url,
                  OpenFileSystemMode mode,
                  ResolveURLCallback callback) override;
  AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) override;
  WatcherManager* GetWatcherManager(FileSystemType type) override;
  CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::File::Error* error_code) override;
  std::unique_ptr<FileSystemOperation> CreateFileSystemOperation(
      OperationType type,
      const FileSystemURL& url,
      FileSystemContext* context,
      base::File::Error* error_code) const override;
  bool SupportsStreaming(const FileSystemURL& url) const override;
  bool HasInplaceCopyImplementation(FileSystemType type) const override;
  std::unique_ptr<FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      FileSystemContext* context,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access) const override;
  std::unique_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64_t offset,
      FileSystemContext* context) const override;
  FileSystemQuotaUtil* GetQuotaUtil() override;
  const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const override;
  const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const override;
  const AccessObserverList* GetAccessObservers(
      FileSystemType type) const override;

  // Initialize the CopyOrMoveFileValidatorFactory. Invalid to call more than
  // once.
  void InitializeCopyOrMoveFileValidatorFactory(
      std::unique_ptr<CopyOrMoveFileValidatorFactory> factory);

  void AddFileChangeObserver(FileChangeObserver* observer);

  // For CopyOrMoveFileValidatorFactory testing. Once it's set to true
  // GetCopyOrMoveFileValidatorFactory will start returning security
  // error if validator is not initialized.
  void set_require_copy_or_move_validator(bool flag) {
    require_copy_or_move_validator_ = flag;
  }

 private:
  class QuotaUtil;

  base::FilePath base_path_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<AsyncFileUtilAdapter> file_util_;
  std::unique_ptr<QuotaUtil> quota_util_;
  UpdateObserverList update_observers_;
  ChangeObserverList change_observers_;

  bool require_copy_or_move_validator_;
  std::unique_ptr<CopyOrMoveFileValidatorFactory>
      copy_or_move_file_validator_factory_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_BACKEND_H_
