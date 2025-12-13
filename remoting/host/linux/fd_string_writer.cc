// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/fd_string_writer.h"

#include <unistd.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/strcat.h"

namespace remoting {

FdStringWriter::~FdStringWriter() = default;

// static
std::unique_ptr<FdStringWriter> FdStringWriter::Write(std::string data,
                                                      base::ScopedFD fd,
                                                      Callback callback) {
  return base::WrapUnique(
      new FdStringWriter(std::move(data), std::move(fd), std::move(callback)));
}

FdStringWriter::FdStringWriter(std::string data,
                               base::ScopedFD fd,
                               Callback callback)
    : fd_(std::move(fd)),
      callback_(std::move(callback)),
      write_data_(std::move(data)) {
  write_remaining_ = write_data_;

  // Unretained is safe because no callback will occur after the controller is
  // destroyed.
  fd_watcher_ = base::FileDescriptorWatcher::WatchWritable(
      fd_.get(), base::BindRepeating(&FdStringWriter::OnFdWritable,
                                     base::Unretained(this)));
}

void FdStringWriter::OnFdWritable() {
  while (true) {
    // Check at the start of the loop, in case the caller asked to write an
    // empty string.
    if (write_remaining_.empty()) {
      // All data was written successfully. Reset the watcher to ensure no more
      // callbacks occur.
      fd_watcher_.reset();
      std::move(callback_).Run(base::ok());
      return;
    }

    ssize_t bytes_written = HANDLE_EINTR(
        write(fd_.get(), write_remaining_.data(), write_remaining_.size()));
    if (bytes_written < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Write was blocked, so return without doing anything. This method will
        // be called again when the FD becomes writable.
        return;
      }
      // A permanent (non-blocking-related) write error occurred. Reset the
      // watcher to ensure no more callbacks occur.
      fd_watcher_.reset();
      std::move(callback_).Run(base::unexpected(Loggable(
          FROM_HERE,
          base::StrCat({"write() failed: ", base::safe_strerror(errno)}))));
      return;
    }
    // bytes_written >= 0. POSIX write() should never return exactly 0 (unless
    // it was asked to write 0 bytes, but this is ruled out by the check at the
    // start).

    // write() should never return more bytes than it was asked to write. Crash
    // here instead of risking undefined behavior on the string_view.
    size_t bytes_written_as_size_t = base::checked_cast<size_t>(bytes_written);
    CHECK_LE(bytes_written_as_size_t, write_remaining_.size());

    // At least some of the data was successfully written, so update the write
    // position. Then continue writing until all data is written or the stream
    // is blocked.
    write_remaining_.remove_prefix(bytes_written_as_size_t);
  }
}

}  // namespace remoting
