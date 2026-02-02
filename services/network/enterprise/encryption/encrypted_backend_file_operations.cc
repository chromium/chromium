// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/encrypted_backend_file_operations.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "net/disk_cache/cache_encryption_delegate.h"
#include "services/network/enterprise/encryption/encrypted_cache_file.h"

namespace network::enterprise_encryption {

UnboundEncryptedBackendFileOperations::UnboundEncryptedBackendFileOperations(
    std::unique_ptr<disk_cache::UnboundBackendFileOperations> decorated_ops,
    const crypto::ProcessBoundString& primary_key)
    : decorated_ops_(std::move(decorated_ops)),
      primary_key_(primary_key) {}

UnboundEncryptedBackendFileOperations::
    ~UnboundEncryptedBackendFileOperations() = default;

std::unique_ptr<disk_cache::BackendFileOperations>
UnboundEncryptedBackendFileOperations::Bind(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return std::make_unique<EncryptedBackendFileOperations>(
      decorated_ops_->Bind(std::move(task_runner)), primary_key_);
}

EncryptedBackendFileOperations::EncryptedBackendFileOperations(
    std::unique_ptr<BackendFileOperations> decorated_backend,
    const crypto::ProcessBoundString& primary_key)
    : decorated_backend_(std::move(decorated_backend)),
      primary_key_(primary_key) {}

EncryptedBackendFileOperations::~EncryptedBackendFileOperations() = default;

bool EncryptedBackendFileOperations::CreateDirectory(
    const base::FilePath& path) {
  return decorated_backend_->CreateDirectory(path);
}

bool EncryptedBackendFileOperations::PathExists(const base::FilePath& path) {
  return decorated_backend_->PathExists(path);
}

bool EncryptedBackendFileOperations::DirectoryExists(
    const base::FilePath& path) {
  return decorated_backend_->DirectoryExists(path);
}

std::unique_ptr<disk_cache::CacheFile> EncryptedBackendFileOperations::OpenFile(
    const base::FilePath& path,
    uint32_t flags) {
  return std::make_unique<EncryptedCacheFile>(
      decorated_backend_->OpenFile(path, flags), primary_key_);
}

bool EncryptedBackendFileOperations::DeleteFile(const base::FilePath& path,
                                                DeleteFileMode mode) {
  return decorated_backend_->DeleteFile(path, mode);
}

bool EncryptedBackendFileOperations::ReplaceFile(
    const base::FilePath& from_path,
    const base::FilePath& to_path,
    base::File::Error* error) {
  return decorated_backend_->ReplaceFile(from_path, to_path, error);
}

std::optional<base::File::Info> EncryptedBackendFileOperations::GetFileInfo(
    const base::FilePath& path) {
  return decorated_backend_->GetFileInfo(path);
}

std::unique_ptr<disk_cache::BackendFileOperations::FileEnumerator>
EncryptedBackendFileOperations::EnumerateFiles(const base::FilePath& path) {
  return decorated_backend_->EnumerateFiles(path);
}

void EncryptedBackendFileOperations::CleanupDirectory(
    const base::FilePath& path,
    base::OnceCallback<void(bool)> callback) {
  decorated_backend_->CleanupDirectory(path, std::move(callback));
}

std::unique_ptr<disk_cache::UnboundBackendFileOperations>
EncryptedBackendFileOperations::Unbind() {
  return std::make_unique<UnboundEncryptedBackendFileOperations>(
      decorated_backend_->Unbind(), primary_key_);
}

bool EncryptedBackendFileOperations::IsEncrypted() const {
  return true;
}

}  // namespace network::enterprise_encryption
