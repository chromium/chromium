// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_LOCAL_FILE_STREAM_WRITER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_LOCAL_FILE_STREAM_WRITER_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "storage/browser/file_system/file_stream_writer.h"

namespace net {
class FileStream;
}

namespace storage {

// This class is a thin wrapper around net::FileStream for writing local files.
class COMPONENT_EXPORT(STORAGE_BROWSER) LocalFileStreamWriter
    : public FileStreamWriter {
 public:
  ~LocalFileStreamWriter() override;

  // FileStreamWriter overrides.
  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback) override;
  int Cancel(net::CompletionOnceCallback callback) override;
  int Flush(net::CompletionOnceCallback callback) override;

 private:
  friend class FileStreamWriter;
  friend class LocalFileStreamWriterTest;

  LocalFileStreamWriter(base::TaskRunner* task_runner,
                        const base::FilePath& file_path,
                        int64_t initial_offset,
                        OpenOrCreate open_or_create);

  // Opens |file_path_| and if it succeeds, proceeds to InitiateSeek().
  // If failed, the error code is returned by calling |error_callback|.
  int InitiateOpen(base::OnceClosure main_operation);
  void DidOpen(base::OnceClosure main_operation, int result);

  // Seeks to |initial_offset_| and proceeds to |main_operation| if it succeeds.
  // If failed, the error code is returned by calling |error_callback|.
  void InitiateSeek(base::OnceClosure main_operation);
  void DidSeek(base::OnceClosure main_operation, int64_t result);

  // Passed as the |main_operation| of InitiateOpen() function.
  void ReadyToWrite(net::IOBuffer* buf, int buf_len);

  // Writes asynchronously to the file.
  int InitiateWrite(net::IOBuffer* buf, int buf_len);
  void DidWrite(int result);

  // Flushes asynchronously to the file.
  int InitiateFlush(net::CompletionOnceCallback callback);
  void DidFlush(net::CompletionOnceCallback callback, int result);

  // Stops the in-flight operation and calls |cancel_callback_| if it has been
  // set by Cancel() for the current operation.
  bool CancelIfRequested();

  // Initialization parameters.
  const base::FilePath file_path_;
  OpenOrCreate open_or_create_;
  const int64_t initial_offset_;
  scoped_refptr<base::TaskRunner> task_runner_;

  // Current states of the operation.
  bool has_pending_operation_;
  std::unique_ptr<net::FileStream> stream_impl_;
  net::CompletionOnceCallback write_callback_;
  net::CompletionOnceCallback cancel_callback_;

  base::WeakPtrFactory<LocalFileStreamWriter> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(LocalFileStreamWriter);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_LOCAL_FILE_STREAM_WRITER_H_
