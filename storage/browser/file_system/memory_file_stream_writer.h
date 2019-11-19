// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_MEMORY_FILE_STREAM_WRITER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_MEMORY_FILE_STREAM_WRITER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"

namespace storage {

// This is a stream writer for in-memory files.
class COMPONENT_EXPORT(STORAGE_BROWSER) MemoryFileStreamWriter
    : public FileStreamWriter {
 public:
  ~MemoryFileStreamWriter() override;

  // FileStreamWriter overrides.
  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback) override;
  int Cancel(net::CompletionOnceCallback callback) override;
  int Flush(net::CompletionOnceCallback callback) override;

 private:
  friend class FileStreamWriter;
  MemoryFileStreamWriter(
      base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
      const base::FilePath& file_path,
      int64_t initial_offset,
      OpenOrCreate open_or_create);

  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_;

  const base::FilePath file_path_;
  int64_t offset_;

  DISALLOW_COPY_AND_ASSIGN(MemoryFileStreamWriter);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_MEMORY_FILE_STREAM_WRITER_H_
