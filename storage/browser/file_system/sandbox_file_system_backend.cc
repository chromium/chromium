// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_system_backend.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/task_runner_util.h"
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
#include "storage/browser/file_system/sandbox_quota_observer.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/gurl.h"

using storage::QuotaManagerProxy;
using storage::SpecialStoragePolicy;

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
  delegate_->RegisterQuotaUpdateObserver(storage::kFileSystemTypeTemporary);
  delegate_->AddFileAccessObserver(storage::kFileSystemTypeTemporary,
                                   delegate_->quota_observer(), nullptr);

  delegate_->RegisterQuotaUpdateObserver(storage::kFileSystemTypePersistent);
  delegate_->AddFileAccessObserver(storage::kFileSystemTypePersistent,
                                   delegate_->quota_observer(), nullptr);
}

void SandboxFileSystemBackend::ResolveURL(const FileSystemURL& url,
                                          OpenFileSystemMode mode,
                                          OpenFileSystemCallback callback) {
  DCHECK(CanHandleType(url.type()));
  DCHECK(delegate_);
  if (delegate_->file_system_options().is_incognito() &&
      url.type() != kFileSystemTypeTemporary &&
      !base::FeatureList::IsEnabled(
          features::kEnablePersistentFilesystemInIncognito)) {
    // TODO(kinuko): return an isolated temporary directory.
    std::move(callback).Run(GURL(), std::string(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  delegate_->OpenFileSystem(
      url.origin().GetURL(), url.type(), mode, std::move(callback),
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

FileSystemOperation* SandboxFileSystemBackend::CreateFileSystemOperation(
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
    operation_context->set_quota_limit_type(storage::kQuotaLimitTypeUnlimited);
  else
    operation_context->set_quota_limit_type(storage::kQuotaLimitTypeLimited);

  return FileSystemOperation::Create(url, context,
                                     std::move(operation_context));
}

bool SandboxFileSystemBackend::SupportsStreaming(
    const storage::FileSystemURL& url) const {
  // Streaming is required for in-memory implementation to access memory-backed
  // files.
  return delegate_->file_system_options().is_incognito();
}

bool SandboxFileSystemBackend::HasInplaceCopyImplementation(
    storage::FileSystemType type) const {
  return false;
}

std::unique_ptr<storage::FileStreamReader>
SandboxFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(delegate_);
  return delegate_->CreateFileStreamReader(url, offset,
                                           expected_modification_time, context);
}

std::unique_ptr<storage::FileStreamWriter>
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

SandboxFileSystemBackendDelegate::OriginEnumerator*
SandboxFileSystemBackend::CreateOriginEnumerator() {
  DCHECK(delegate_);
  return delegate_->CreateOriginEnumerator();
}

}  // namespace storage
