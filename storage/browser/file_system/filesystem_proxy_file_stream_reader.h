// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILESYSTEM_PROXY_FILE_STREAM_READER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILESYSTEM_PROXY_FILE_STREAM_READER_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"

namespace base {
class TaskRunner;
}

namespace net {
class FileStream;
}

namespace storage {

// A thin wrapper of net::FileStream with range support for sliced file
// handling.
class COMPONENT_EXPORT(STORAGE_BROWSER) FilesystemProxyFileStreamReader
    : public FileStreamReader {
 public:
  FilesystemProxyFileStreamReader(
      scoped_refptr<base::TaskRunner> task_runner,
      const base::FilePath& file_path,
      std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
      int64_t initial_offset,
      const base::Time& expected_modification_time,
      bool emit_metrics,
      base::PassKey<FileStreamReader> pass_key);
  ~FilesystemProxyFileStreamReader() override;

  // FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

  using SharedFilesystemProxy =
      base::RefCountedData<std::unique_ptr<storage::FilesystemProxy>>;

 private:
  void Open(net::CompletionOnceCallback callback);

  // Callbacks that are chained from Open for Read.
  void DidVerifyForOpen(net::CompletionOnceCallback callback,
                        int64_t get_length_result);
  void DidOpenFile(base::FileErrorOr<base::File> result);

  void DidSeekFileStream(int64_t seek_result);
  void DidOpenForRead(net::IOBuffer* buf,
                      int buf_len,
                      net::CompletionOnceCallback callback,
                      int open_result);
  void OnRead(int read_result);

  void DidGetFileInfoForGetLength(net::Int64CompletionOnceCallback callback,
                                  base::FileErrorOr<base::File::Info> result);

  net::CompletionOnceCallback callback_;
  scoped_refptr<base::TaskRunner> task_runner_;
  std::unique_ptr<net::FileStream> stream_impl_;
  // FilesystemProxy is threadsafe, however all file operations must be
  // run on |task_runner_|.  Therefore, this refcounted-data preserves
  // the lifetime of the filesystem proxy even if the FileStreamReader is
  // destroyed during a callback.
  scoped_refptr<SharedFilesystemProxy> shared_filesystem_proxy_;
  const base::FilePath file_path_;
  const int64_t initial_offset_;
  const base::Time expected_modification_time_;
  bool has_pending_open_ = false;
  bool emit_metrics_ = false;
  base::WeakPtrFactory<FilesystemProxyFileStreamReader> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILESYSTEM_PROXY_FILE_STREAM_READER_H_
