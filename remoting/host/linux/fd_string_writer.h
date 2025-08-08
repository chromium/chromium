// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_FD_STRING_WRITER_H_
#define REMOTING_HOST_LINUX_FD_STRING_WRITER_H_

#include <memory>
#include <string>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"

namespace remoting {

// Class to write a string asynchronously to a file-descriptor. The object
// returned by Write() will own the file-descriptor, closing it on destruction.
class FdStringWriter {
 public:
  using Callback = base::OnceCallback<void(base::expected<void, Loggable>)>;

  FdStringWriter() = delete;
  FdStringWriter(const FdStringWriter&) = delete;
  FdStringWriter& operator=(const FdStringWriter&) = delete;
  ~FdStringWriter();

  static std::unique_ptr<FdStringWriter> Write(std::string data,
                                               base::ScopedFD fd,
                                               Callback callback);

 private:
  FdStringWriter(std::string data, base::ScopedFD fd, Callback callback);

  void OnFdWritable();

  // The file-descriptor must outlast the watcher, so it is declared first.
  base::ScopedFD fd_;
  Callback callback_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> fd_watcher_;
  std::string write_data_;

  // Stores the current write position, up to the end of the string.
  std::string_view write_remaining_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_FD_STRING_WRITER_H_
