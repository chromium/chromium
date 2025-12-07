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
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/numerics/checked_math.h"
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
  FileBackgroundIO(disk_cache::File* file,
                   base::span<uint8_t> buffer,
                   size_t offset,
                   disk_cache::FileIOCallback* callback,
                   disk_cache::InFlightIO* controller)
      : disk_cache::BackgroundIO(controller),
        callback_(callback),
        file_(file),
        buffer_(buffer),
        offset_(offset) {}

  FileBackgroundIO(const FileBackgroundIO&) = delete;
  FileBackgroundIO& operator=(const FileBackgroundIO&) = delete;

  disk_cache::FileIOCallback* ReleaseCallback() {
    return callback_.ExtractAsDangling();
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

  raw_ptr<disk_cache::File, DanglingUntriaged> file_;
  base::raw_span<uint8_t, DanglingUntriaged> buffer_;
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
  void PostRead(disk_cache::File* file,
                base::span<uint8_t> buffer,
                size_t offset,
                disk_cache::FileIOCallback* callback);
  void PostWrite(disk_cache::File* file,
                 base::span<uint8_t> buffer,
                 size_t offset,
                 disk_cache::FileIOCallback* callback);

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
  if (file_->Read(buffer_, offset_)) {
    result_ = buffer_.size();
  } else {
    result_ = net::ERR_CACHE_READ_FAILURE;
  }
  NotifyController();
}

// Runs on a worker thread.
void FileBackgroundIO::Write() {
  bool rv = file_->Write(buffer_, offset_);

  result_ = rv ? buffer_.size() : net::ERR_CACHE_WRITE_FAILURE;
  NotifyController();
}

// ---------------------------------------------------------------------------

void FileInFlightIO::PostRead(disk_cache::File* file,
                              base::span<uint8_t> buffer,
                              size_t offset,
                              disk_cache::FileIOCallback* callback) {
  auto operation = base::MakeRefCounted<FileBackgroundIO>(file, buffer, offset,
                                                          callback, this);
  file->AddRef();  // Balanced on OnOperationComplete()

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FileBackgroundIO::Read, operation.get()));
  OnOperationPosted(operation.get());
}

void FileInFlightIO::PostWrite(disk_cache::File* file,
                               base::span<uint8_t> buffer,
                               size_t offset,
                               disk_cache::FileIOCallback* callback) {
  auto operation = base::MakeRefCounted<FileBackgroundIO>(file, buffer, offset,
                                                          callback, this);
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

  int bytes = operation->result();

  // Release the references acquired in PostRead / PostWrite.
  op->file()->Release();

  // The callback may be be deleted by the `OnFileIOComplete` call below,
  // and we also won't need it ourselves after this.
  // TODO(morlovich): It may be better to refactor this so that the callback is
  // just owned here; that would require splitting ChildDeleter to have rather
  // than be one. See
  // https://chromium-review.googlesource.com/c/chromium/src/+/6426561/2..3/net/disk_cache/blockfile/file_ios.cc#b45
  op->ReleaseCallback()->OnFileIOComplete(bytes);
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

bool File::Read(base::span<uint8_t> buffer, size_t offset) {
  DCHECK(base_file_.IsValid());
  if (!base::IsValueInRangeForNumericType<int32_t>(buffer.size()) ||
      !base::IsValueInRangeForNumericType<int32_t>(offset)) {
    return false;
  }

  std::optional<size_t> ret = base_file_.Read(offset, buffer);
  return ret == buffer.size();
}

bool File::Write(base::span<const uint8_t> buffer, size_t offset) {
  DCHECK(base_file_.IsValid());
  if (!base::IsValueInRangeForNumericType<int32_t>(buffer.size()) ||
      !base::IsValueInRangeForNumericType<int32_t>(offset)) {
    return false;
  }

  std::optional<size_t> ret = base_file_.Write(offset, buffer);
  return ret == buffer.size();
}

// We have to increase the ref counter of the file before performing the IO to
// prevent the completion to happen with an invalid handle (if the file is
// closed while the IO is in flight).
bool File::Read(base::span<uint8_t> buffer,
                size_t offset,
                FileIOCallback* callback,
                bool* completed) {
  DCHECK(base_file_.IsValid());
  if (!callback) {
    if (completed)
      *completed = true;
    return Read(buffer, offset);
  }

  if (offset > ULONG_MAX) {
    return false;
  }

  GetFileInFlightIO()->PostRead(this, buffer, offset, callback);

  *completed = false;
  return true;
}

bool File::Write(base::span<const uint8_t> buffer,
                 size_t offset,
                 FileIOCallback* callback,
                 bool* completed) {
  DCHECK(base_file_.IsValid());
  if (!callback) {
    if (completed)
      *completed = true;
    return Write(buffer, offset);
  }

  if (offset > ULONG_MAX) {
    return false;
  }

  GetFileInFlightIO()->PostWrite(
      this,
      // SAFETY: Converting `base::span<const uint8_t>` to `base::span<uint8_t>`
      // does not involve any other changes.
      UNSAFE_BUFFERS(
          base::span(const_cast<uint8_t*>(buffer.data()), buffer.size())),
      offset, callback);
  if (completed) {
    *completed = false;
  }
  return true;
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

}  // namespace disk_cache
