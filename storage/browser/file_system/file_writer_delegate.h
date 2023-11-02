// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_WRITER_DELEGATE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_WRITER_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "storage/browser/blob/blob_reader.h"

namespace storage {

class FileStreamWriter;
enum class FlushPolicy;

class COMPONENT_EXPORT(STORAGE_BROWSER) FileWriterDelegate {
 public:
  enum WriteProgressStatus {
    SUCCESS_IO_PENDING,
    SUCCESS_COMPLETED,
    ERROR_WRITE_STARTED,
    ERROR_WRITE_NOT_STARTED,
  };

  using DelegateWriteCallback =
      base::RepeatingCallback<void(base::File::Error result,
                                   int64_t bytes,
                                   WriteProgressStatus write_status)>;

  FileWriterDelegate(std::unique_ptr<FileStreamWriter> file_writer,
                     FlushPolicy flush_policy);

  FileWriterDelegate(const FileWriterDelegate&) = delete;
  FileWriterDelegate& operator=(const FileWriterDelegate&) = delete;

  virtual ~FileWriterDelegate();

  void Start(std::unique_ptr<BlobReader> blob_reader,
             DelegateWriteCallback write_callback);
  void Start(mojo::ScopedDataPipeConsumerHandle data_pipe,
             DelegateWriteCallback write_callback);

  // Cancels the current write operation.  This will synchronously or
  // asynchronously call the given write callback (which may result in
  // deleting this).
  void Cancel();

 protected:
  // Virtual for tests.
  virtual void OnDataReceived(int bytes_read);

 private:
  void OnDidCalculateSize(int net_error);
  void Read();
  void OnReadCompleted(int bytes_read);
  void Write();
  void OnDataWritten(int write_response);
  void OnReadError(base::File::Error error);
  void OnWriteError(base::File::Error error);
  void OnProgress(int bytes_read, bool done);
  void OnWriteCancelled(int status);
  void MaybeFlushForCompletion(base::File::Error error,
                               int bytes_written,
                               WriteProgressStatus progress_status);
  void OnFlushed(base::File::Error error,
                 int bytes_written,
                 WriteProgressStatus progress_status,
                 int flush_error);

  void OnDataPipeReady(MojoResult result,
                       const mojo::HandleSignalsState& state);

  WriteProgressStatus GetCompletionStatusOnError() const;

  // Note: This object can be deleted after calling this callback.
  DelegateWriteCallback write_callback_;
  std::unique_ptr<FileStreamWriter> file_stream_writer_;
  base::Time last_progress_event_time_;
  bool writing_started_;
  FlushPolicy flush_policy_;
  int bytes_written_backlog_;
  int bytes_written_;
  int bytes_read_;
  bool async_write_in_progress_ = false;
  base::File::Error saved_read_error_ = base::File::FILE_OK;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;
  scoped_refptr<net::DrainableIOBuffer> cursor_;

  // Used when reading from a blob.
  std::unique_ptr<BlobReader> blob_reader_;

  // Used when reading from a data pipe.
  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  mojo::SimpleWatcher data_pipe_watcher_;

  base::WeakPtrFactory<FileWriterDelegate> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_WRITER_DELEGATE_H_
