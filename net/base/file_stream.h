// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines FileStream, a basic interface for reading and writing files
// synchronously or asynchronously with support for seeking to an offset.
// Note that even when used asynchronously, only one operation is supported at
// a time.

#ifndef NET_BASE_FILE_STREAM_H_
#define NET_BASE_FILE_STREAM_H_

#include <stdint.h>

#include <memory>

#include "base/byte_size.h"
#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"

namespace base {
class FilePath;
class TaskRunner;
}

namespace net {

class IOBuffer;

class NET_EXPORT FileStream {
 public:
  // Callback type for Read() and Write() - returns either the number of bytes
  // transferred (as base::ByteSize) on success or a net::Error (never net::OK)
  // on failure.
  using ReadWriteCallback =
      base::OnceCallback<void(base::expected<base::ByteSize, net::Error>)>;

  // Callback type for operations that only report success (net::OK) or an
  // error code: Open(), Close(), Flush(), GetFileInfo(), ConnectNamedPipe().
  using ErrorCallback = base::OnceCallback<void(net::Error)>;

  // Callback type for Seek() - returns either the new file position on success
  // or a net::Error on failure.
  using SeekCallback =
      base::OnceCallback<void(base::expected<int64_t, net::Error>)>;

  // Uses |task_runner| for asynchronous operations.
  explicit FileStream(const scoped_refptr<base::TaskRunner>& task_runner);

  // Construct a FileStream with an already opened file. |file| must be opened
  // for async reading on Windows, and sync reading everywehere else. To use
  // ConnectNamedPipe(), `file` must wrap a handle from CreateNamedPipe() with
  // `FILE_FLAG_OVERLAPPED`.
  //
  // Uses |task_runner| for asynchronous operations.
  FileStream(base::File file,
             const scoped_refptr<base::TaskRunner>& task_runner);

  FileStream(const FileStream&) = delete;
  FileStream& operator=(const FileStream&) = delete;
  // The underlying file is closed automatically.
  virtual ~FileStream();

  // Call this method to open the FileStream asynchronously.  The remaining
  // methods cannot be used unless the file is opened successfully. Returns
  // ERR_IO_PENDING if the operation is started. If the operation cannot be
  // started then an error code is returned.
  //
  // Once the operation is done, |callback| will be run on the thread where
  // Open() was called, with the result code. open_flags is a bitfield of
  // base::File::Flags and must include FLAG_ASYNC.
  //
  // If the file stream is not closed manually, the underlying file will be
  // automatically closed when FileStream is destructed in an asynchronous
  // manner (i.e. the file stream is closed in the background but you don't
  // know when).
  virtual int Open(const base::FilePath& path,
                   int open_flags,
                   ErrorCallback callback);

  // Returns ERR_IO_PENDING and closes the file asynchronously, calling
  // |callback| when done.
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  virtual int Close(ErrorCallback callback);

  // Returns true if Open succeeded and Close has not been called.
  virtual bool IsOpen() const;

  // Adjust the position from the start of the file where data is read
  // asynchronously. Upon success, ERR_IO_PENDING is returned and |callback|
  // will be run on the thread where Seek() was called with the the stream
  // position relative to the start of the file.  Otherwise, an error code is
  // returned. It is invalid to request any asynchronous operations while there
  // is an in-flight asynchronous operation.
  virtual int Seek(int64_t offset, SeekCallback callback);

  // Call this method to read data from the current stream position
  // asynchronously. Up to buf_len bytes will be copied into buf.  (In
  // other words, partial reads are allowed.)  If the operation completes
  // synchronously, the result (the number of bytes copied as base::ByteSize,
  // 0 if at end-of-file, or a net::Error (never net::OK)) is returned. If
  // the operation does not complete synchronously, ERR_IO_PENDING is returned
  // (as an error), and the callback is called asynchronously with the result.
  //
  // A non-null callback must be passed to this method.
  //
  // It is valid to destroy or close the file stream while there is an
  // asynchronous read in progress.  That will cancel the read and allow
  // the buffer to be freed.
  //
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  //
  // This method must not be called if the stream was opened WRITE_ONLY.
  virtual base::expected<base::ByteSize, net::Error>
  Read(IOBuffer* buf, int buf_len, ReadWriteCallback callback);

  // Call this method to write data at the current stream position
  // asynchronously.  Up to buf_len bytes will be written from buf. (In
  // other words, partial writes are allowed.)  If the operation completes
  // synchronously, the result (the number of bytes written as base::ByteSize,
  // or a net::Error (never net::OK)) is returned. If the operation does not
  // complete synchronously, ERR_IO_PENDING is returned (as an error), and the
  // callback is called asynchronously with the result.
  //
  // A non-null callback must be passed to this method.
  //
  // It is valid to destroy or close the file stream while there is an
  // asynchronous write in progress.  That will cancel the write and allow
  // the buffer to be freed.
  //
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  //
  // This method must not be called if the stream was opened READ_ONLY.
  //
  // Zero byte writes are not allowed.
  virtual base::expected<base::ByteSize, net::Error>
  Write(IOBuffer* buf, int buf_len, ReadWriteCallback callback);

  // Gets status information about File. May fail synchronously, but never
  // succeeds synchronously.
  //
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  //
  // |file_info| must remain valid until |callback| is invoked.
  virtual int GetFileInfo(base::File::Info* file_info, ErrorCallback callback);

  // Forces out a filesystem sync on this file to make sure that the file was
  // written out to disk and is not currently sitting in the buffer. This does
  // not have to be called, it just forces one to happen at the time of
  // calling.
  //
  // The file must be opened with FLAG_ASYNC, and a non-null
  // callback must be passed to this method. If the write could not
  // complete synchronously, then ERR_IO_PENDING is returned, and the
  // callback will be run on the thread where Flush() was called when
  // the write has completed.
  //
  // It is valid to destroy or close the file stream while there is an
  // asynchronous flush in progress.  That will cancel the flush and allow
  // the buffer to be freed.
  //
  // It is invalid to request any asynchronous operations while there is an
  // in-flight asynchronous operation.
  //
  // This method should not be called if the stream was opened READ_ONLY.
  virtual int Flush(ErrorCallback callback);

#if BUILDFLAG(IS_WIN)
  // Waits for a client to connect to the named pipe provided to the two-arg
  // constructor. Returns `OK` if the client has already connected,
  // `ERR_IO_PENDING` if `callback` will be run later with the result of the
  // operation once the client connects, or another `Error` value in case of
  // failure.
  int ConnectNamedPipe(ErrorCallback callback);
#endif  // BUILDFLAG(IS_WIN)

 private:
  class Context;

  // Context performing I/O operations. It was extracted into a separate class
  // to perform asynchronous operations because FileStream can be destroyed
  // before completion of an async operation. Also if a FileStream is destroyed
  // without explicitly calling Close, the file should be closed asynchronously
  // without delaying FileStream's destructor.
  std::unique_ptr<Context> context_;
};

}  // namespace net

#endif  // NET_BASE_FILE_STREAM_H_
