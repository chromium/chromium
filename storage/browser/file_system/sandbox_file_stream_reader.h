// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_STREAM_READER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_STREAM_READER_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"

namespace base {
class FilePath;
}

namespace storage {

class FileSystemContext;

// FileStreamReader implementation for Sandboxed FileSystem files.
// This wraps either a LocalFileStreamReader or a MemoryFileStreamReader,
// depending on if we're in an incognito profile or not.
class COMPONENT_EXPORT(STORAGE_BROWSER) SandboxFileStreamReader
    : public FileStreamReader {
 public:
  // Creates a new reader for a filesystem URL |url| from |initial_offset|.
  // |expected_modification_time| specifies the expected last modification. if
  // the value is non-null, the reader will check the underlying file's actual
  // modification time to see if the file has been modified, and if it does any
  // succeeding read operations should fail with ERR_UPLOAD_FILE_CHANGED error.
  SandboxFileStreamReader(FileSystemContext* file_system_context,
                          const FileSystemURL& url,
                          int64_t initial_offset,
                          const base::Time& expected_modification_time);

  SandboxFileStreamReader(const SandboxFileStreamReader&) = delete;
  SandboxFileStreamReader& operator=(const SandboxFileStreamReader&) = delete;

  ~SandboxFileStreamReader() override;

  // FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

 private:
  friend class FileStreamReader;
  using SnapshotCallback = FileSystemOperationRunner::SnapshotFileCallback;

  int CreateSnapshot(SnapshotCallback callback);
  void DidCreateSnapshotForRead(net::IOBuffer* read_buf,
                                int read_len,
                                net::CompletionOnceCallback callback,
                                base::File::Error file_error,
                                const base::File::Info& file_info,
                                const base::FilePath& platform_path,
                                scoped_refptr<ShareableFileReference> file_ref);
  void DidCreateSnapshotForGetLength(
      net::Int64CompletionOnceCallback callback,
      base::File::Error file_error,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      scoped_refptr<ShareableFileReference> file_ref);
  void CreateFileReader(const base::FilePath& platform_path);

  void OnRead(net::CompletionOnceCallback callback, int rv);
  void OnGetLength(net::Int64CompletionOnceCallback callback, int64_t rv);

  scoped_refptr<FileSystemContext> file_system_context_;
  FileSystemURL url_;
  const int64_t initial_offset_;
  const base::Time expected_modification_time_;
  std::unique_ptr<FileStreamReader> file_reader_;
  scoped_refptr<ShareableFileReference> snapshot_ref_;
  bool has_pending_create_snapshot_;
  base::WeakPtrFactory<SandboxFileStreamReader> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_STREAM_READER_H_
