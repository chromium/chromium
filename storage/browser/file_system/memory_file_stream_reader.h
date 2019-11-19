// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_MEMORY_FILE_STREAM_READER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_MEMORY_FILE_STREAM_READER_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"

namespace storage {

// A stream reader for memory files.
class COMPONENT_EXPORT(STORAGE_BROWSER) MemoryFileStreamReader
    : public FileStreamReader {
 public:
  ~MemoryFileStreamReader() override;

  // FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

 private:
  friend class FileStreamReader;

  MemoryFileStreamReader(
      base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
      const base::FilePath& file_path,
      int64_t initial_offset,
      const base::Time& expected_modification_time);

  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_;

  const base::FilePath file_path_;
  const base::Time expected_modification_time_;
  int64_t offset_;

  DISALLOW_COPY_AND_ASSIGN(MemoryFileStreamReader);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_MEMORY_FILE_STREAM_READER_H_
