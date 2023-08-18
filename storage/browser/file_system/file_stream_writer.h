// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_WRITER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_WRITER_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"

namespace base {
class FilePath;
class TaskRunner;
}  // namespace base

namespace net {
class IOBuffer;
}

namespace storage {

// Indicates whether the flush is done in the middle or at the end of a file.
enum class FlushMode {
  kDefault,
  kEndOfFile,
};

// A generic interface for writing to a file-like object.
class FileStreamWriter {
 public:
  enum OpenOrCreate {
    OPEN_EXISTING_FILE,
    CREATE_NEW_FILE,
    CREATE_NEW_FILE_ALWAYS
  };

  // Creates a writer for the existing file in the path |file_path| starting
  // from |initial_offset|. Uses |task_runner| for async file operations.
  COMPONENT_EXPORT(STORAGE_BROWSER)
  static std::unique_ptr<FileStreamWriter> CreateForLocalFile(
      base::TaskRunner* task_runner,
      const base::FilePath& file_path,
      int64_t initial_offset,
      OpenOrCreate open_or_create);

  FileStreamWriter(const FileStreamWriter&) = delete;
  FileStreamWriter& operator=(const FileStreamWriter&) = delete;
  // Closes the file. If there's an in-flight operation, it is canceled (i.e.,
  // the callback function associated with the operation is not called).
  virtual ~FileStreamWriter() = default;

  // Writes to the current cursor position asynchronously.
  //
  // Up to buf_len bytes will be written.  (In other words, partial
  // writes are allowed.) If the write completed synchronously, it returns
  // the number of bytes written. If the operation could not be performed, it
  // returns an error code. Otherwise, net::ERR_IO_PENDING is returned, and the
  // callback will be run on the thread where Write() was called when the write
  // has completed.
  //
  // After the last write, Flush() must be called if the file system written to
  // was registered with the FlushPolicy::FLUSH_ON_COMPLETION policy in mount
  // options.
  //
  // This errors out (either synchronously or via callback) with:
  //   net::ERR_FILE_NOT_FOUND: When the target file is not found.
  //   net::ERR_ACCESS_DENIED: When the target file is a directory or
  //      the writer has no permission on the file.
  //   net::ERR_FILE_NO_SPACE: When the write will result in out of quota
  //      or there is not enough room left on the disk.
  //
  // It is invalid to call Write while there is an in-flight async operation.
  virtual int Write(net::IOBuffer* buf,
                    int buf_len,
                    net::CompletionOnceCallback callback) = 0;

  // Cancels an in-flight async operation.
  //
  // If the cancel is finished synchronously, it returns net::OK. If the
  // cancel could not be performed, it returns an error code. Especially when
  // there is no in-flight operation, net::ERR_UNEXPECTED is returned.
  // Otherwise, net::ERR_IO_PENDING is returned, and the callback will be run on
  // the thread where Cancel() was called when the cancel has completed. It is
  // invalid to call Cancel() more than once on the same async operation.
  //
  // In either case, the callback function passed to the in-flight async
  // operation is dismissed immediately when Cancel() is called, and thus
  // will never be called.
  virtual int Cancel(net::CompletionOnceCallback callback) = 0;

  // Flushes the data written so far.
  //
  // The flush_mode argument exists because some implementations may be backed
  // by cloud-storage APIs (not local disk) that do not support incremental
  // writes. In such cases, only the final flush does an upload and any earlier
  // flushes should be no-ops, but the caller still needs to tell the callee
  // which flush is the final one.
  //
  // Some other "stream writer" APIs have separate Flush and Close methods, but
  // for historical reasons, this API has Flush(FlushMode::kDefault) and
  // Flush(FlushMode::kEndOfFile). Note that Flush(FlushMode::kEndOfFile), the
  // equivalent of Close, still takes a callback (it can involve async I/O),
  // unlike the FileStreamWriter destructor (as destructors have no arguments).
  //
  // If the flush finished synchronously, it return net::OK. If the flush could
  // not be performed, it returns an error code. Otherwise, net::ERR_IO_PENDING
  // is returned, and the callback will be run on the thread where Flush() was
  // called when the flush has completed.
  //
  // It is invalid to call Flush while there is an in-flight async operation.
  virtual int Flush(FlushMode flush_mode,
                    net::CompletionOnceCallback callback) = 0;

 protected:
  FileStreamWriter() = default;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_WRITER_H_
