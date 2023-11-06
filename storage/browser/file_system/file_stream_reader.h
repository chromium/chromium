// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_READER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_READER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "net/base/completion_once_callback.h"

namespace base {
class FilePath;
class TaskRunner;
class Time;
}  // namespace base

namespace net {
class IOBuffer;
}

namespace storage {

// A generic interface for reading a file-like object.
class FileStreamReader {
 public:
  // Creates a new FileReader for a local file |file_path|.
  // |initial_offset| specifies the offset in the file where the first read
  // should start.  If the given offset is out of the file range any
  // read operation may error out with net::ERR_REQUEST_RANGE_NOT_SATISFIABLE.
  // |expected_modification_time| specifies the expected last modification
  // If the value is non-null, the reader will check the underlying file's
  // actual modification time to see if the file has been modified, and if
  // it does any succeeding read operations should fail with
  // ERR_UPLOAD_FILE_CHANGED error.
  COMPONENT_EXPORT(STORAGE_BROWSER)
  static std::unique_ptr<FileStreamReader> CreateForLocalFile(
      scoped_refptr<base::TaskRunner> task_runner,
      const base::FilePath& file_path,
      int64_t initial_offset,
      const base::Time& expected_modification_time,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access = base::NullCallback());

  // Verify if the underlying file has not been modified.
  COMPONENT_EXPORT(STORAGE_BROWSER)
  static bool VerifySnapshotTime(const base::Time& expected_modification_time,
                                 const base::File::Info& file_info);

  // It is valid to delete the reader at any time.  If the stream is deleted
  // while it has a pending read, its callback will not be called.
  virtual ~FileStreamReader() = default;

  // Reads from the current cursor position asynchronously.
  //
  // Up to buf_len bytes will be copied into buf.  (In other words, partial
  // reads are allowed.)  Returns the number of bytes copied, 0 if at
  // end-of-file, or an error code if the operation could not be performed.
  // If the read could not complete synchronously, then ERR_IO_PENDING is
  // returned, and the callback will be run on the thread where Read()
  // was called, when the read has completed.
  //
  // It is invalid to call Read while there is an in-flight Read operation.
  //
  // If the stream is deleted while it has an in-flight Read operation
  // |callback| will not be called.
  virtual int Read(net::IOBuffer* buf,
                   int buf_len,
                   net::CompletionOnceCallback callback) = 0;

  // Returns the length of the file if it could successfully retrieve the
  // file info *and* its last modification time equals to
  // expected modification time (rv >= 0 cases).
  // Otherwise, a negative error code is returned (rv < 0 cases).
  // If the stream is deleted while it has an in-flight GetLength operation
  // |callback| will not be called.
  // Note that the return type is int64_t to return a larger file's size (a file
  // larger than 2G) but an error code should fit in the int range (may be
  // smaller than int64_t range).
  virtual int64_t GetLength(net::Int64CompletionOnceCallback callback) = 0;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_READER_H_
