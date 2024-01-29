// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DISK_CACHE_MOJO_BACKEND_FILE_OPERATIONS_H_
#define SERVICES_NETWORK_DISK_CACHE_MOJO_BACKEND_FILE_OPERATIONS_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/disk_cache/disk_cache.h"
#include "services/network/public/mojom/http_cache_backend_file_operations.mojom.h"

namespace network {

// A BackendFileOperations that provides file operations with brokering them
// via mojo.
class MojoBackendFileOperations final
    : public disk_cache::BackendFileOperations {
 public:
  MojoBackendFileOperations(
      mojo::PendingRemote<mojom::HttpCacheBackendFileOperations> pending_remote,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~MojoBackendFileOperations() override;

  // disk_cache::BackendFileOperations implementation:
  bool CreateDirectory(const base::FilePath& path) override;
  bool PathExists(const base::FilePath& path) override;
  bool DirectoryExists(const base::FilePath& path) override;
  base::File OpenFile(const base::FilePath& path, uint32_t flags) override;
  bool DeleteFile(const base::FilePath& path, DeleteFileMode mode) override;
  bool ReplaceFile(const base::FilePath& from_path,
                   const base::FilePath& to_path,
                   base::File::Error* error) override;
  std::optional<base::File::Info> GetFileInfo(
      const base::FilePath& path) override;
  std::unique_ptr<disk_cache::BackendFileOperations::FileEnumerator>
  EnumerateFiles(const base::FilePath& path) override;
  void CleanupDirectory(const base::FilePath& path,
                        base::OnceCallback<void(bool)> callback) override;
  std::unique_ptr<disk_cache::UnboundBackendFileOperations> Unbind() override;

 private:
  mojo::Remote<mojom::HttpCacheBackendFileOperations> remote_;
};

class UnboundMojoBackendFileOperations final
    : public disk_cache::UnboundBackendFileOperations {
 public:
  explicit UnboundMojoBackendFileOperations(
      mojo::PendingRemote<mojom::HttpCacheBackendFileOperations>
          pending_remote);
  ~UnboundMojoBackendFileOperations() override;

  std::unique_ptr<disk_cache::BackendFileOperations> Bind(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;

 private:
  mojo::PendingRemote<mojom::HttpCacheBackendFileOperations> pending_remote_;
};

}  // namespace network
#endif  // SERVICES_NETWORK_DISK_CACHE_MOJO_BACKEND_FILE_OPERATIONS_H_
