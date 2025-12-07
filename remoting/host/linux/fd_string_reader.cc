// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/fd_string_reader.h"

#include <unistd.h>

#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"

namespace remoting {

namespace {

// Linux pipes typically have a buffer size of 64k, though this can vary.
constexpr size_t kBufferSize = 1U << 16;

FdStringReader::Result ReadContentsOfFile(base::ScopedFD fd) {
  base::File file(fd.release());
  base::MemoryMappedFile mapped_file;
  if (!mapped_file.Initialize(std::move(file))) {
    return base::unexpected(
        Loggable(FROM_HERE, "Failed to initialize MemoryMappedFile"));
  }
  auto contents = base::as_chars(mapped_file.bytes());
  return std::string(contents.begin(), contents.end());
}

}  // namespace

FdStringReader::~FdStringReader() = default;

// static
std::unique_ptr<FdStringReader> FdStringReader::ReadFromPipe(
    base::ScopedFD fd,
    Callback callback) {
  return base::WrapUnique(
      new FdStringReader(std::move(fd), std::move(callback)));
}

// static
std::unique_ptr<FdStringReader> FdStringReader::ReadFromFile(
    base::ScopedFD fd,
    Callback callback) {
  auto reader = base::WrapUnique(new FdStringReader(std::move(callback)));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadContentsOfFile, std::move(fd)),
      base::BindOnce(&FdStringReader::OnReadComplete,
                     reader->weak_factory_.GetWeakPtr()));
  return reader;
}

FdStringReader::FdStringReader(base::ScopedFD fd, Callback callback)
    : fd_(std::move(fd)), callback_(std::move(callback)) {
  // As an optimization, reserve buffer space now, so that the first
  // memory-allocation happens before the FD becomes readable (if it's not
  // already readable). Call reserve() instead of resize(), so that
  // read_data_ stays logically empty. This respects the invariant that
  // read_data_ holds exactly the data returned by all of the read() calls.
  read_data_.reserve(kBufferSize);

  // Unretained is safe because no callback will occur after the controller is
  // destroyed.
  fd_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      fd_.get(), base::BindRepeating(&FdStringReader::OnFdReadable,
                                     base::Unretained(this)));
}

FdStringReader::FdStringReader(Callback callback)
    : callback_(std::move(callback)) {}

void FdStringReader::OnFdReadable() {
  while (true) {
    // Reserve space for new data. Depending on the read() result, the string
    // will be restored to its original size, or shrunk to just include the
    // amount of data read.
    size_t original_size = read_data_.size();
    size_t new_size = base::CheckAdd(original_size, kBufferSize).ValueOrDie();
    read_data_.resize(new_size);
    auto writable_part =
        base::as_writable_byte_span(read_data_).last(kBufferSize);

    ssize_t bytes_read = HANDLE_EINTR(
        read(fd_.get(), writable_part.data(), writable_part.size()));
    if (bytes_read < 0) {
      read_data_.resize(original_size);
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Read was blocked, so return without doing anything. This method will
        // be called again when the FD becomes readable.
        return;
      }
      // A permanent (non-blocking-related) read error occurred. Reset the
      // watcher to ensure no more callbacks occur.
      fd_watcher_.reset();
      std::move(callback_).Run(base::unexpected(Loggable(
          FROM_HERE,
          base::StrCat({"read() failed: ", base::safe_strerror(errno)}))));
      return;
    }
    if (bytes_read == 0) {
      // End-of-stream reached. Reset the watcher to ensure no more callbacks
      // occur.
      read_data_.resize(original_size);
      fd_watcher_.reset();
      std::move(callback_).Run(base::ok(std::move(read_data_)));
      return;
    }
    // bytes_read > 0.

    // read() should never return more data than the buffer size. Crash here
    // instead of resizing the string beyond the reserved buffer.
    size_t bytes_read_as_size_t = base::checked_cast<size_t>(bytes_read);
    CHECK_LE(bytes_read_as_size_t, kBufferSize);

    // Store the data, and continue reading as much as possible. The addition of
    // kBufferSize was already checked so it's safe to add a smaller number
    // here without CheckAdd().
    read_data_.resize(original_size + bytes_read_as_size_t);
  }
}

void FdStringReader::OnReadComplete(Result result) {
  std::move(callback_).Run(std::move(result));
}

}  // namespace remoting
