// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/encrypted_backend_file_operations.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"

namespace network::enterprise {

UnboundEncryptedBackendFileOperations::UnboundEncryptedBackendFileOperations(
    std::unique_ptr<disk_cache::UnboundBackendFileOperations> decorated_ops)
    : decorated_ops_(std::move(decorated_ops)) {}

UnboundEncryptedBackendFileOperations::
    ~UnboundEncryptedBackendFileOperations() = default;

std::unique_ptr<disk_cache::BackendFileOperations>
UnboundEncryptedBackendFileOperations::Bind(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return std::make_unique<EncryptedBackendFileOperations>(
      decorated_ops_->Bind(std::move(task_runner)));
}

EncryptedBackendFileOperations::EncryptedBackendFileOperations(
    std::unique_ptr<BackendFileOperations> decorated_backend)
    : decorated_backend_(std::move(decorated_backend)) {}

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

base::File EncryptedBackendFileOperations::OpenFile(const base::FilePath& path,
                                                    uint32_t flags) {
  return decorated_backend_->OpenFile(path, flags);
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
      decorated_backend_->Unbind());
}

bool EncryptedBackendFileOperations::IsEncrypted() const {
  return true;
}

}  // namespace network::enterprise
