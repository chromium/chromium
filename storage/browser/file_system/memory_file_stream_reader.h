// Copyright 2019 The Chromium Authors
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
  // Creates a new FileReader for a memory file |file_path|.
  // |initial_offset| specifies the offset in the file where the first read
  // should start.  If the given offset is out of the file range any
  // read operation may error out with net::ERR_REQUEST_RANGE_NOT_SATISFIABLE.
  // |expected_modification_time| specifies the expected last modification
  // If the value is non-null, the reader will check the underlying file's
  // actual modification time to see if the file has been modified, and if
  // it does any succeeding read operations should fail with
  // ERR_UPLOAD_FILE_CHANGED error.
  MemoryFileStreamReader(
      scoped_refptr<base::TaskRunner> task_runner,
      base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
      const base::FilePath& file_path,
      int64_t initial_offset,
      const base::Time& expected_modification_time);

  MemoryFileStreamReader(const MemoryFileStreamReader&) = delete;
  MemoryFileStreamReader& operator=(const MemoryFileStreamReader&) = delete;

  ~MemoryFileStreamReader() override;

  // FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

 private:
  void OnReadCompleted(net::CompletionOnceCallback callback, int result);
  void OnGetLengthCompleted(net::Int64CompletionOnceCallback callback,
                            int64_t result);

  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_;

  const scoped_refptr<base::TaskRunner> task_runner_;
  const base::FilePath file_path_;
  const base::Time expected_modification_time_;
  int64_t offset_;

  base::WeakPtrFactory<MemoryFileStreamReader> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_MEMORY_FILE_STREAM_READER_H_
