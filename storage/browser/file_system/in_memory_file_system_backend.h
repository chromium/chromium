// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_IN_MEMORY_FILE_SYSTEM_BACKEND_H_
#define STORAGE_BROWSER_FILE_SYSTEM_IN_MEMORY_FILE_SYSTEM_BACKEND_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "storage/browser/file_system/file_system_backend.h"

namespace storage {

// Serves `kFileSystemTypeInMemory` files, whose contents are held by
// InMemoryFileRegistry and never touch the disk.
//
// The backend is read-only: it supports streaming reads (which is what blobs
// created from a `filesystem:` URL need) and metadata lookups. Other
// operations that a caller can reach, e.g. mutations, directory listings and
// snapshots, fail with an error. Operations that no caller can reach, e.g.
// opening a base::File or creating a writer, are NOTREACHED().
//
// One instance is created per StoragePartition via
// ContentBrowserClient::GetAdditionalFileSystemBackends(), but the files
// themselves live in the process-global InMemoryFileRegistry.
class COMPONENT_EXPORT(STORAGE_BROWSER) InMemoryFileSystemBackend
    : public FileSystemBackend {
 public:
  InMemoryFileSystemBackend();

  InMemoryFileSystemBackend(const InMemoryFileSystemBackend&) = delete;
  InMemoryFileSystemBackend& operator=(const InMemoryFileSystemBackend&) =
      delete;

  ~InMemoryFileSystemBackend() override;

  // FileSystemBackend:
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

 private:
  class AsyncFileUtilImpl;

  std::unique_ptr<AsyncFileUtilImpl> async_file_util_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_IN_MEMORY_FILE_SYSTEM_BACKEND_H_
