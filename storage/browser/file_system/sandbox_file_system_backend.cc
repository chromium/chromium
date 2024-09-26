// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_system_backend.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "storage/browser/file_system/async_file_util_adapter.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_features.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_options.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/file_system/quota/quota_limit_type.h"
#include "storage/browser/file_system/sandbox_quota_observer.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/gurl.h"

namespace storage {

SandboxFileSystemBackend::SandboxFileSystemBackend(
    SandboxFileSystemBackendDelegate* delegate)
    : delegate_(delegate) {}

SandboxFileSystemBackend::~SandboxFileSystemBackend() = default;

bool SandboxFileSystemBackend::CanHandleType(FileSystemType type) const {
  return type == kFileSystemTypeTemporary || type == kFileSystemTypePersistent;
}

void SandboxFileSystemBackend::Initialize(FileSystemContext* context) {
  DCHECK(delegate_);

  // Set quota observers.
  delegate_->RegisterQuotaUpdateObserver(kFileSystemTypeTemporary);
  delegate_->AddFileAccessObserver(kFileSystemTypeTemporary,
                                   delegate_->quota_observer(), nullptr);

  delegate_->RegisterQuotaUpdateObserver(kFileSystemTypePersistent);
  delegate_->AddFileAccessObserver(kFileSystemTypePersistent,
                                   delegate_->quota_observer(), nullptr);
}

void SandboxFileSystemBackend::ResolveURL(const FileSystemURL& url,
                                          OpenFileSystemMode mode,
                                          ResolveURLCallback callback) {
  DCHECK(CanHandleType(url.type()));
  DCHECK(delegate_);

  delegate_->OpenFileSystem(
      url.GetBucket(), url.type(), mode, std::move(callback),
      GetFileSystemRootURI(url.origin().GetURL(), url.type()));
}

AsyncFileUtil* SandboxFileSystemBackend::GetAsyncFileUtil(FileSystemType type) {
  DCHECK(delegate_);
  return delegate_->file_util();
}

WatcherManager* SandboxFileSystemBackend::GetWatcherManager(
    FileSystemType type) {
  return nullptr;
}

CopyOrMoveFileValidatorFactory*
SandboxFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return nullptr;
}

std::unique_ptr<FileSystemOperation>
SandboxFileSystemBackend::CreateFileSystemOperation(
    OperationType type,
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(error_code);

  DCHECK(delegate_);
  std::unique_ptr<FileSystemOperationContext> operation_context =
      delegate_->CreateFileSystemOperationContext(url, context, error_code);
  if (!operation_context)
    return nullptr;

  SpecialStoragePolicy* policy = delegate_->special_storage_policy();
  if (policy && policy->IsStorageUnlimited(url.origin().GetURL()))
    operation_context->set_quota_limit_type(QuotaLimitType::kUnlimited);
  else
    operation_context->set_quota_limit_type(QuotaLimitType::kLimited);

  return FileSystemOperation::Create(type, url, context,
                                     std::move(operation_context));
}

bool SandboxFileSystemBackend::SupportsStreaming(
    const FileSystemURL& url) const {
  // Streaming is required for in-memory implementation to access memory-backed
  // files.
  return delegate_->file_system_options().is_incognito();
}

bool SandboxFileSystemBackend::HasInplaceCopyImplementation(
    FileSystemType type) const {
  return false;
}

std::unique_ptr<FileStreamReader>
SandboxFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    FileSystemContext* context,
    file_access::ScopedFileAccessDelegate::
        RequestFilesAccessIOCallback /*file_access*/) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(delegate_);
  return delegate_->CreateFileStreamReader(url, offset,
                                           expected_modification_time, context);
}

std::unique_ptr<FileStreamWriter>
SandboxFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64_t offset,
    FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(delegate_);
  return delegate_->CreateFileStreamWriter(url, offset, context, url.type());
}

FileSystemQuotaUtil* SandboxFileSystemBackend::GetQuotaUtil() {
  return delegate_;
}

const UpdateObserverList* SandboxFileSystemBackend::GetUpdateObservers(
    FileSystemType type) const {
  return delegate_->GetUpdateObservers(type);
}

const ChangeObserverList* SandboxFileSystemBackend::GetChangeObservers(
    FileSystemType type) const {
  return delegate_->GetChangeObservers(type);
}

const AccessObserverList* SandboxFileSystemBackend::GetAccessObservers(
    FileSystemType type) const {
  return delegate_->GetAccessObservers(type);
}

SandboxFileSystemBackendDelegate::StorageKeyEnumerator*
SandboxFileSystemBackend::CreateStorageKeyEnumerator() {
  DCHECK(delegate_);
  return delegate_->CreateStorageKeyEnumerator();
}

}  // namespace storage
