// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_FILE_WRITER_IMPL_H_
#define STORAGE_BROWSER_FILEAPI_FILE_WRITER_IMPL_H_

#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/storage_browser_export.h"
#include "third_party/blink/public/mojom/filesystem/file_writer.mojom.h"

namespace storage {

// This class itself is stateless, but it uses the passed in
// FileSystemOperationRunner and BlobStorageContext so all methods should be
// called on the sequence those instances live on. In chromium that means all
// usage has to be on the IO thread.
class STORAGE_EXPORT FileWriterImpl : public blink::mojom::FileWriter {
 public:
  FileWriterImpl(FileSystemURL url,
                 std::unique_ptr<FileSystemOperationRunner> operation_runner,
                 base::WeakPtr<BlobStorageContext> blob_context);
  ~FileWriterImpl() override;

  void Write(uint64_t position,
             blink::mojom::BlobPtr blob,
             WriteCallback callback) override;
  void WriteStream(uint64_t position,
                   mojo::ScopedDataPipeConsumerHandle stream,
                   WriteStreamCallback callback) override;
  void Truncate(uint64_t length, TruncateCallback callback) override;

 private:
  void DoWrite(WriteCallback callback,
               uint64_t position,
               std::unique_ptr<BlobDataHandle> blob);
  void DoWriteWithFileInfo(WriteCallback callback,
                           uint64_t position,
                           std::unique_ptr<BlobDataHandle> blob,
                           base::File::Error result,
                           const base::File::Info& file_info);
  void DoWriteStreamWithFileInfo(WriteCallback callback,
                                 uint64_t position,
                                 mojo::ScopedDataPipeConsumerHandle data_pipe,
                                 base::File::Error result,
                                 const base::File::Info& file_info);

  struct WriteState {
    uint64_t bytes_written = 0;
  };
  void DidWrite(WriteCallback callback,
                WriteState* state,
                base::File::Error result,
                int64_t bytes,
                bool complete);
  void DidTruncate(TruncateCallback callback, base::File::Error result);

  const std::unique_ptr<FileSystemOperationRunner> operation_runner_;
  const base::WeakPtr<BlobStorageContext> blob_context_;
  const FileSystemURL url_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_FILE_WRITER_IMPL_H_
