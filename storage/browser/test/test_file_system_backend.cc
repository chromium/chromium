// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/test_file_system_backend.h"

#include <set>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/browser/file_system/local_file_util.h"
#include "storage/browser/file_system/native_file_util.h"
#include "storage/browser/file_system/quota/quota_reservation.h"
#include "storage/browser/file_system/sandbox_file_stream_reader.h"
#include "storage/browser/file_system/sandbox_file_stream_writer.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/file_system/file_system_util.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

namespace {

// Stub implementation of LocalFileUtil.
class TestFileUtil : public LocalFileUtil {
 public:
  explicit TestFileUtil(const base::FilePath& base_path)
      : base_path_(base_path) {}
  ~TestFileUtil() override = default;

  // LocalFileUtil overrides.
  base::File::Error GetLocalFilePath(FileSystemOperationContext* context,
                                     const FileSystemURL& file_system_url,
                                     base::FilePath* local_file_path) override {
    *local_file_path = base_path_.Append(file_system_url.path());
    return base::File::FILE_OK;
  }

 private:
  base::FilePath base_path_;
};

}  // namespace

// This only supports single origin.
class TestFileSystemBackend::QuotaUtil : public FileSystemQuotaUtil,
                                         public FileUpdateObserver {
 public:
  QuotaUtil() : usage_(0) {}

  QuotaUtil(const QuotaUtil&) = delete;
  QuotaUtil& operator=(const QuotaUtil&) = delete;

  ~QuotaUtil() override = default;

  // FileSystemQuotaUtil overrides.
  void DeleteCachedDefaultBucket(
      const blink::StorageKey& storage_key) override {
    NOTREACHED();
  }

  base::File::Error DeleteBucketDataOnFileTaskRunner(
      FileSystemContext* context,
      QuotaManagerProxy* proxy,
      const BucketLocator& bucket_locator,
      FileSystemType type) override {
    NOTREACHED();
  }

  void PerformStorageCleanupOnFileTaskRunner(FileSystemContext* context,
                                             QuotaManagerProxy* proxy,
                                             FileSystemType type) override {}

  scoped_refptr<QuotaReservation> CreateQuotaReservationOnFileTaskRunner(
      const blink::StorageKey& storage_key,
      FileSystemType type) override {
    NOTREACHED();
  }

  std::vector<blink::StorageKey> GetStorageKeysForTypeOnFileTaskRunner(
      FileSystemType type) override {
    NOTREACHED();
  }

  int64_t GetBucketUsageOnFileTaskRunner(FileSystemContext* context,
                                         const BucketLocator& bucket_locator,
                                         FileSystemType type) override {
    return usage_;
  }

  // FileUpdateObserver overrides.
  void OnStartUpdate(const FileSystemURL& url) override {}
  void OnUpdate(const FileSystemURL& url, int64_t delta) override {
    usage_ += delta;
  }
  void OnEndUpdate(const FileSystemURL& url) override {}

 private:
  int64_t usage_;
};

TestFileSystemBackend::TestFileSystemBackend(
    base::SequencedTaskRunner* task_runner,
    const base::FilePath& base_path)
    : base_path_(base_path),
      task_runner_(task_runner),
      file_util_(std::make_unique<AsyncFileUtilAdapter>(
          std::make_unique<TestFileUtil>(base_path))),
      quota_util_(std::make_unique<QuotaUtil>()),
      require_copy_or_move_validator_(false) {
  update_observers_ =
      update_observers_.AddObserver(quota_util_.get(), task_runner_.get());
}

TestFileSystemBackend::~TestFileSystemBackend() = default;

bool TestFileSystemBackend::CanHandleType(FileSystemType type) const {
  return (type == kFileSystemTypeTest);
}

void TestFileSystemBackend::Initialize(FileSystemContext* context) {}

void TestFileSystemBackend::ResolveURL(const FileSystemURL& url,
                                       OpenFileSystemMode mode,
                                       ResolveURLCallback callback) {
  std::move(callback).Run(
      GetFileSystemRootURI(url.origin().GetURL(), url.type()),
      GetFileSystemName(url.origin().GetURL(), url.type()),
      base::File::FILE_OK);
}

AsyncFileUtil* TestFileSystemBackend::GetAsyncFileUtil(FileSystemType type) {
  return file_util_.get();
}

WatcherManager* TestFileSystemBackend::GetWatcherManager(FileSystemType type) {
  return nullptr;
}

CopyOrMoveFileValidatorFactory*
TestFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  if (require_copy_or_move_validator_) {
    if (!copy_or_move_file_validator_factory_)
      *error_code = base::File::FILE_ERROR_SECURITY;
    return copy_or_move_file_validator_factory_.get();
  }
  return nullptr;
}

void TestFileSystemBackend::InitializeCopyOrMoveFileValidatorFactory(
    std::unique_ptr<CopyOrMoveFileValidatorFactory> factory) {
  if (!copy_or_move_file_validator_factory_)
    copy_or_move_file_validator_factory_ = std::move(factory);
}

std::unique_ptr<FileSystemOperation>
TestFileSystemBackend::CreateFileSystemOperation(
    OperationType type,
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  std::unique_ptr<FileSystemOperationContext> operation_context(
      std::make_unique<FileSystemOperationContext>(context));
  operation_context->set_update_observers(*GetUpdateObservers(url.type()));
  operation_context->set_change_observers(*GetChangeObservers(url.type()));
  return FileSystemOperation::Create(type, url, context,
                                     std::move(operation_context));
}

bool TestFileSystemBackend::SupportsStreaming(const FileSystemURL& url) const {
  return false;
}

bool TestFileSystemBackend::HasInplaceCopyImplementation(
    FileSystemType type) const {
  return true;
}

std::unique_ptr<FileStreamReader> TestFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    FileSystemContext* context,
    file_access::ScopedFileAccessDelegate::
        RequestFilesAccessIOCallback /*file_access*/) const {
  return std::make_unique<SandboxFileStreamReader>(context, url, offset,
                                                   expected_modification_time);
}

std::unique_ptr<FileStreamWriter> TestFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64_t offset,
    FileSystemContext* context) const {
  return std::make_unique<SandboxFileStreamWriter>(
      context, url, offset, *GetUpdateObservers(url.type()));
}

FileSystemQuotaUtil* TestFileSystemBackend::GetQuotaUtil() {
  return quota_util_.get();
}

const UpdateObserverList* TestFileSystemBackend::GetUpdateObservers(
    FileSystemType type) const {
  return &update_observers_;
}

const ChangeObserverList* TestFileSystemBackend::GetChangeObservers(
    FileSystemType type) const {
  return &change_observers_;
}

const AccessObserverList* TestFileSystemBackend::GetAccessObservers(
    FileSystemType type) const {
  return nullptr;
}

void TestFileSystemBackend::AddFileChangeObserver(
    FileChangeObserver* observer) {
  change_observers_ =
      change_observers_.AddObserver(observer, task_runner_.get());
}

}  // namespace storage
