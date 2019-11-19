// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_BACKEND_H_
#define STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_BACKEND_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
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

namespace content {

// This should be only used for testing.
// This file system backend uses LocalFileUtil and stores data file
// under the given directory.
class TestFileSystemBackend : public storage::FileSystemBackend {
 public:
  TestFileSystemBackend(base::SequencedTaskRunner* task_runner,
                        const base::FilePath& base_path);
  ~TestFileSystemBackend() override;

  // FileSystemBackend implementation.
  bool CanHandleType(storage::FileSystemType type) const override;
  void Initialize(storage::FileSystemContext* context) override;
  void ResolveURL(const storage::FileSystemURL& url,
                  storage::OpenFileSystemMode mode,
                  OpenFileSystemCallback callback) override;
  storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) override;
  storage::WatcherManager* GetWatcherManager(
      storage::FileSystemType type) override;
  storage::CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      storage::FileSystemType type,
      base::File::Error* error_code) override;
  storage::FileSystemOperation* CreateFileSystemOperation(
      const storage::FileSystemURL& url,
      storage::FileSystemContext* context,
      base::File::Error* error_code) const override;
  bool SupportsStreaming(const storage::FileSystemURL& url) const override;
  bool HasInplaceCopyImplementation(
      storage::FileSystemType type) const override;
  std::unique_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) const override;
  std::unique_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64_t offset,
      storage::FileSystemContext* context) const override;
  storage::FileSystemQuotaUtil* GetQuotaUtil() override;
  const storage::UpdateObserverList* GetUpdateObservers(
      storage::FileSystemType type) const override;
  const storage::ChangeObserverList* GetChangeObservers(
      storage::FileSystemType type) const override;
  const storage::AccessObserverList* GetAccessObservers(
      storage::FileSystemType type) const override;

  // Initialize the CopyOrMoveFileValidatorFactory. Invalid to call more than
  // once.
  void InitializeCopyOrMoveFileValidatorFactory(
      std::unique_ptr<storage::CopyOrMoveFileValidatorFactory> factory);

  void AddFileChangeObserver(storage::FileChangeObserver* observer);

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
  std::unique_ptr<storage::AsyncFileUtilAdapter> file_util_;
  std::unique_ptr<QuotaUtil> quota_util_;
  storage::UpdateObserverList update_observers_;
  storage::ChangeObserverList change_observers_;

  bool require_copy_or_move_validator_;
  std::unique_ptr<storage::CopyOrMoveFileValidatorFactory>
      copy_or_move_file_validator_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestFileSystemBackend);
};

}  // namespace content

#endif  // STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_BACKEND_H_
