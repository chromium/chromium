// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_FILE_STREAM_READER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_FILE_STREAM_READER_H_

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/component_export.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_url.h"

namespace base {
class FilePath;
}

namespace storage {

class FileSystemContext;

// Generic FileStreamReader implementation for FileSystem files.
// Note: This generic implementation would work for any filesystems but
// remote filesystem should implement its own reader rather than relying
// on FileSystemOperation::GetSnapshotFile() which may force downloading
// the entire contents for remote files.
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemFileStreamReader
    : public FileStreamReader {
 public:
  ~FileSystemFileStreamReader() override;

  // FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

 private:
  friend class FileStreamReader;
  friend class FileSystemFileStreamReaderTest;

  FileSystemFileStreamReader(FileSystemContext* file_system_context,
                             const FileSystemURL& url,
                             int64_t initial_offset,
                             const base::Time& expected_modification_time);

  int CreateSnapshot();
  void DidCreateSnapshot(base::File::Error file_error,
                         const base::File::Info& file_info,
                         const base::FilePath& platform_path,
                         scoped_refptr<ShareableFileReference> file_ref);
  void OnRead(int rv);
  void OnGetLength(int64_t rv);

  net::IOBuffer* read_buf_;
  int read_buf_len_;
  net::CompletionOnceCallback read_callback_;
  net::Int64CompletionOnceCallback get_length_callback_;
  scoped_refptr<FileSystemContext> file_system_context_;
  FileSystemURL url_;
  const int64_t initial_offset_;
  const base::Time expected_modification_time_;
  std::unique_ptr<FileStreamReader> file_reader_;
  scoped_refptr<ShareableFileReference> snapshot_ref_;
  bool has_pending_create_snapshot_;
  base::WeakPtrFactory<FileSystemFileStreamReader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FileSystemFileStreamReader);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_FILE_STREAM_READER_H_
