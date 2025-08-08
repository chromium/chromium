// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_FD_STRING_READER_H_
#define REMOTING_HOST_LINUX_FD_STRING_READER_H_

#include <memory>
#include <string>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"

namespace remoting {

// Class to read asynchronously from a file-descriptor to a std::string, until
// end-of-stream is reached or an error occurs. The object returned by Read()
// will own the file-descriptor, closing it on destruction.
class FdStringReader {
 public:
  using Callback =
      base::OnceCallback<void(base::expected<std::string, Loggable>)>;

  FdStringReader() = delete;
  FdStringReader(const FdStringReader&) = delete;
  FdStringReader& operator=(const FdStringReader&) = delete;
  ~FdStringReader();

  static std::unique_ptr<FdStringReader> Read(base::ScopedFD fd,
                                              Callback callback);

 private:
  FdStringReader(base::ScopedFD fd, Callback callback);

  void OnFdReadable();

  // The file-descriptor must outlast the watcher, so it is declared first.
  base::ScopedFD fd_;
  Callback callback_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> fd_watcher_;
  std::string read_data_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_FD_STRING_READER_H_
