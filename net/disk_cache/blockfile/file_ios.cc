// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/file.h"

#include <limits.h>
#include <stdint.h>

#include <limits>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/blockfile/in_flight_io.h"
#include "net/disk_cache/disk_cache.h"

namespace {

// This class represents a single asynchronous IO operation while it is being
// bounced between threads.
class FileBackgroundIO : public disk_cache::BackgroundIO {
 public:
  // Other than the actual parameters for the IO operation (including the
  // |callback| that must be notified at the end), we need the controller that
  // is keeping track of all operations. When done, we notify the controller
  // (we do NOT invoke the callback), in the worker thead that completed the
  // operation.
  FileBackgroundIO(disk_cache::File* file, const void* buf, size_t buf_len,
                   size_t offset, disk_cache::FileIOCallback* callback,
                   disk_cache::InFlightIO* controller)
      : disk_cache::BackgroundIO(controller), callback_(callback), file_(file),
        buf_(buf), buf_len_(buf_len), offset_(offset) {
  }

  FileBackgroundIO(const FileBackgroundIO&) = delete;
  FileBackgroundIO& operator=(const FileBackgroundIO&) = delete;

  disk_cache::FileIOCallback* callback() {
    return callback_;
  }

  disk_cache::File* file() {
    return file_;
  }

  // Read and Write are the operations that can be performed asynchronously.
  // The actual parameters for the operation are setup in the constructor of
  // the object. Both methods should be called from a worker thread, by posting
  // a task to the WorkerPool (they are RunnableMethods). When finished,
  // controller->OnIOComplete() is called.
  void Read();
  void Write();

 private:
  ~FileBackgroundIO() override {}

  raw_ptr<disk_cache::FileIOCallback> callback_;

  raw_ptr<disk_cache::File> file_;
  raw_ptr<const void> buf_;
  size_t buf_len_;
  size_t offset_;
};


// The specialized controller that keeps track of current operations.
class FileInFlightIO : public disk_cache::InFlightIO {
 public:
  FileInFlightIO() = default;

  FileInFlightIO(const FileInFlightIO&) = delete;
  FileInFlightIO& operator=(const FileInFlightIO&) = delete;

  ~FileInFlightIO() override = default;

  // These methods start an asynchronous operation. The arguments have the same
  // semantics of the File asynchronous operations, with the exception that the
  // operation never finishes synchronously.
  void PostRead(disk_cache::File* file, void* buf, size_t buf_len,
                size_t offset, disk_cache::FileIOCallback* callback);
  void PostWrite(disk_cache::File* file, const void* buf, size_t buf_len,
                 size_t offset, disk_cache::FileIOCallback* callback);

 protected:
  // Invokes the users' completion callback at the end of the IO operation.
  // |cancel| is true if the actual task posted to the thread is still
  // queued (because we are inside WaitForPendingIO), and false if said task is
  // the one performing the call.
  void OnOperationComplete(disk_cache::BackgroundIO* operation,
                           bool cancel) override;
};

// ---------------------------------------------------------------------------

// Runs on a worker thread.
void FileBackgroundIO::Read() {
  if (file_->Read(const_cast<void*>(buf_.get()), buf_len_, offset_)) {
    result_ = static_cast<int>(buf_len_);
  } else {
    result_ = net::ERR_CACHE_READ_FAILURE;
  }
  NotifyController();
}

// Runs on a worker thread.
void FileBackgroundIO::Write() {
  bool rv = file_->Write(buf_, buf_len_, offset_);

  result_ = rv ? static_cast<int>(buf_len_) : net::ERR_CACHE_WRITE_FAILURE;
  NotifyController();
}

// ---------------------------------------------------------------------------

void FileInFlightIO::PostRead(disk_cache::File *file, void* buf, size_t buf_len,
                          size_t offset, disk_cache::FileIOCallback *callback) {
  auto operation = base::MakeRefCounted<FileBackgroundIO>(
      file, buf, buf_len, offset, callback, this);
  file->AddRef();  // Balanced on OnOperationComplete()

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FileBackgroundIO::Read, operation.get()));
  OnOperationPosted(operation.get());
}

void FileInFlightIO::PostWrite(disk_cache::File* file, const void* buf,
                           size_t buf_len, size_t offset,
                           disk_cache::FileIOCallback* callback) {
  auto operation = base::MakeRefCounted<FileBackgroundIO>(
      file, buf, buf_len, offset, callback, this);
  file->AddRef();  // Balanced on OnOperationComplete()

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FileBackgroundIO::Write, operation.get()));
  OnOperationPosted(operation.get());
}

// Runs on the IO thread.
void FileInFlightIO::OnOperationComplete(disk_cache::BackgroundIO* operation,
                                         bool cancel) {
  FileBackgroundIO* op = static_cast<FileBackgroundIO*>(operation);

  disk_cache::FileIOCallback* callback = op->callback();
  int bytes = operation->result();

  // Release the references acquired in PostRead / PostWrite.
  op->file()->Release();
  callback->OnFileIOComplete(bytes);
}

// A static object that will broker all async operations.
FileInFlightIO* s_file_operations = nullptr;

// Returns the current FileInFlightIO.
FileInFlightIO* GetFileInFlightIO() {
  if (!s_file_operations) {
    s_file_operations = new FileInFlightIO;
  }
  return s_file_operations;
}

// Deletes the current FileInFlightIO.
void DeleteFileInFlightIO() {
  DCHECK(s_file_operations);
  delete s_file_operations;
  s_file_operations = nullptr;
}

}  // namespace

namespace disk_cache {

File::File(base::File file)
    : init_(true), mixed_(true), base_file_(std::move(file)) {}

bool File::Init(const base::FilePath& name) {
  if (base_file_.IsValid())
    return false;

  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE;
  base_file_.Initialize(name, flags);
  return base_file_.IsValid();
}

bool File::IsValid() const {
  return base_file_.IsValid();
}

bool File::Read(void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(base_file_.IsValid());
  if (buffer_len > static_cast<size_t>(std::numeric_limits<int32_t>::max()) ||
      offset > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return false;
  }

  int ret = UNSAFE_TODO(
      base_file_.Read(offset, static_cast<char*>(buffer), buffer_len));
  return (static_cast<size_t>(ret) == buffer_len);
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(base_file_.IsValid());
  if (buffer_len > static_cast<size_t>(std::numeric_limits<int32_t>::max()) ||
      offset > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return false;
  }

  int ret = UNSAFE_TODO(
      base_file_.Write(offset, static_cast<const char*>(buffer), buffer_len));
  return (static_cast<size_t>(ret) == buffer_len);
}

// We have to increase the ref counter of the file before performing the IO to
// prevent the completion to happen with an invalid handle (if the file is
// closed while the IO is in flight).
bool File::Read(void* buffer, size_t buffer_len, size_t offset,
                FileIOCallback* callback, bool* completed) {
  DCHECK(base_file_.IsValid());
  if (!callback) {
    if (completed)
      *completed = true;
    return Read(buffer, buffer_len, offset);
  }

  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  GetFileInFlightIO()->PostRead(this, buffer, buffer_len, offset, callback);

  *completed = false;
  return true;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset,
                 FileIOCallback* callback, bool* completed) {
  DCHECK(base_file_.IsValid());
  if (!callback) {
    if (completed)
      *completed = true;
    return Write(buffer, buffer_len, offset);
  }

  return AsyncWrite(buffer, buffer_len, offset, callback, completed);
}

bool File::SetLength(size_t length) {
  DCHECK(base_file_.IsValid());
  if (length > std::numeric_limits<uint32_t>::max())
    return false;

  return base_file_.SetLength(length);
}

size_t File::GetLength() {
  DCHECK(base_file_.IsValid());
  int64_t len = base_file_.GetLength();

  if (len < 0)
    return 0;
  if (len > static_cast<int64_t>(std::numeric_limits<uint32_t>::max()))
    return std::numeric_limits<uint32_t>::max();

  return static_cast<size_t>(len);
}

// Static.
void File::WaitForPendingIOForTesting(int* num_pending_io) {
  // We may be running unit tests so we should allow be able to reset the
  // message loop.
  GetFileInFlightIO()->WaitForPendingIO();
  DeleteFileInFlightIO();
}

// Static.
void File::DropPendingIO() {
  GetFileInFlightIO()->DropPendingIO();
  DeleteFileInFlightIO();
}

File::~File() = default;

base::PlatformFile File::platform_file() const {
  return base_file_.GetPlatformFile();
}

bool File::AsyncWrite(const void* buffer, size_t buffer_len, size_t offset,
                      FileIOCallback* callback, bool* completed) {
  DCHECK(base_file_.IsValid());
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  GetFileInFlightIO()->PostWrite(this, buffer, buffer_len, offset, callback);

  if (completed)
    *completed = false;
  return true;
}

}  // namespace disk_cache
