// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_MEMORY_FILE_STREAM_WRITER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_MEMORY_FILE_STREAM_WRITER_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"

namespace storage {

// This is a stream writer for in-memory files.
class COMPONENT_EXPORT(STORAGE_BROWSER) MemoryFileStreamWriter
    : public FileStreamWriter {
 public:
  MemoryFileStreamWriter(
      scoped_refptr<base::TaskRunner> task_runner,
      base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
      const base::FilePath& file_path,
      int64_t initial_offset);

  MemoryFileStreamWriter(const MemoryFileStreamWriter&) = delete;
  MemoryFileStreamWriter& operator=(const MemoryFileStreamWriter&) = delete;

  ~MemoryFileStreamWriter() override;

  // FileStreamWriter overrides.
  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback) override;
  int Cancel(net::CompletionOnceCallback callback) override;
  int Flush(FlushMode flush_mode,
            net::CompletionOnceCallback callback) override;

 private:
  void OnWriteCompleted(net::CompletionOnceCallback callback, int result);

  // Stops the in-flight operation and calls |cancel_callback_| if it has been
  // set by Cancel() for the current operation.
  bool CancelIfRequested();

  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_;

  const scoped_refptr<base::TaskRunner> task_runner_;
  const base::FilePath file_path_;
  int64_t offset_;

  bool has_pending_operation_;
  net::CompletionOnceCallback cancel_callback_;

  base::WeakPtrFactory<MemoryFileStreamWriter> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_MEMORY_FILE_STREAM_WRITER_H_
