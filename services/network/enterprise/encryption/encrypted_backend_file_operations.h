// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_BACKEND_FILE_OPERATIONS_H_
#define SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_BACKEND_FILE_OPERATIONS_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "crypto/process_bound_string.h"
#include "net/disk_cache/cache_file.h"
#include "net/disk_cache/disk_cache.h"

namespace network::enterprise_encryption {

class UnboundEncryptedBackendFileOperations final
    : public disk_cache::UnboundBackendFileOperations {
 public:
  explicit UnboundEncryptedBackendFileOperations(
      std::unique_ptr<disk_cache::UnboundBackendFileOperations> decorated_ops,
      const crypto::ProcessBoundString& primary_key);
  ~UnboundEncryptedBackendFileOperations() override;

  std::unique_ptr<disk_cache::BackendFileOperations> Bind(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;

 private:
  std::unique_ptr<disk_cache::UnboundBackendFileOperations> decorated_ops_;
  const crypto::ProcessBoundString primary_key_;
};

// Decorator to add encryption layer to file operations.
class EncryptedBackendFileOperations final
    : public disk_cache::BackendFileOperations {
 public:
  explicit EncryptedBackendFileOperations(
      std::unique_ptr<disk_cache::BackendFileOperations> decorated_backend,
      const crypto::ProcessBoundString& primary_key);
  ~EncryptedBackendFileOperations() override;

  bool CreateDirectory(const base::FilePath& path) override;
  bool PathExists(const base::FilePath& path) override;
  bool DirectoryExists(const base::FilePath& path) override;
  std::unique_ptr<disk_cache::CacheFile> OpenFile(const base::FilePath& path,
                                                  uint32_t flags) override;
  bool DeleteFile(const base::FilePath& path, DeleteFileMode mode) override;
  bool ReplaceFile(const base::FilePath& from_path,
                   const base::FilePath& to_path,
                   base::File::Error* error) override;
  std::optional<base::File::Info> GetFileInfo(
      const base::FilePath& path) override;
  std::unique_ptr<FileEnumerator> EnumerateFiles(
      const base::FilePath& path) override;
  void CleanupDirectory(const base::FilePath& path,
                        base::OnceCallback<void(bool)> callback) override;
  std::unique_ptr<disk_cache::UnboundBackendFileOperations> Unbind() override;
  bool IsEncrypted() const override;

 private:
  std::unique_ptr<disk_cache::BackendFileOperations> decorated_backend_;
  const crypto::ProcessBoundString primary_key_;
};

}  // namespace network::enterprise_encryption

#endif  // SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_BACKEND_FILE_OPERATIONS_H_
