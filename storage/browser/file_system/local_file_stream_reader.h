// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_LOCAL_FILE_STREAM_READER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_LOCAL_FILE_STREAM_READER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"

namespace base {
class TaskRunner;
}

namespace content {
class LocalFileStreamReaderTest;
}

namespace net {
class FileStream;
}

namespace storage {

// A thin wrapper of net::FileStream with range support for sliced file
// handling.
class COMPONENT_EXPORT(STORAGE_BROWSER) LocalFileStreamReader
    : public FileStreamReader {
 public:
  ~LocalFileStreamReader() override;

  // FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

 private:
  friend class FileStreamReader;
  friend class content::LocalFileStreamReaderTest;

  LocalFileStreamReader(base::TaskRunner* task_runner,
                        const base::FilePath& file_path,
                        int64_t initial_offset,
                        const base::Time& expected_modification_time);
  void Open(net::CompletionOnceCallback callback);

  // Callbacks that are chained from Open for Read.
  void DidVerifyForOpen(net::CompletionOnceCallback callback,
                        int64_t get_length_result);
  void DidOpenFileStream(int result);
  void DidSeekFileStream(int64_t seek_result);
  void DidOpenForRead(net::IOBuffer* buf,
                      int buf_len,
                      net::CompletionOnceCallback callback,
                      int open_result);
  void OnRead(int read_result);

  void DidGetFileInfoForGetLength(net::Int64CompletionOnceCallback callback,
                                  base::File::Error error,
                                  const base::File::Info& file_info);

  net::CompletionOnceCallback callback_;
  scoped_refptr<base::TaskRunner> task_runner_;
  std::unique_ptr<net::FileStream> stream_impl_;
  const base::FilePath file_path_;
  const int64_t initial_offset_;
  const base::Time expected_modification_time_;
  bool has_pending_open_;
  base::WeakPtrFactory<LocalFileStreamReader> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_LOCAL_FILE_STREAM_READER_H_
