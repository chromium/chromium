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
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"

namespace remoting {

// Class to read asynchronously from a file-descriptor to a std::string, until
// end-of-stream is reached or an error occurs. The object returned by Read()
// will own the file-descriptor, closing it on destruction.
class FdStringReader {
 public:
  using Result = base::expected<std::string, Loggable>;
  using Callback = base::OnceCallback<void(Result)>;

  FdStringReader() = delete;
  FdStringReader(const FdStringReader&) = delete;
  FdStringReader& operator=(const FdStringReader&) = delete;
  ~FdStringReader();

  // For non-blocking file descriptors (e.g. pipes).
  static std::unique_ptr<FdStringReader> ReadFromPipe(base::ScopedFD fd,
                                                      Callback callback);

  // For regular files.
  static std::unique_ptr<FdStringReader> ReadFromFile(base::ScopedFD fd,
                                                      Callback callback);

 private:
  // For Read().
  FdStringReader(base::ScopedFD fd, Callback callback);

  // For ReadFromFile().
  explicit FdStringReader(Callback callback);

  void OnFdReadable();
  void OnReadComplete(Result result);

  // The file-descriptor must outlast the watcher, so it is declared first.
  base::ScopedFD fd_;
  Callback callback_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> fd_watcher_;
  std::string read_data_;

  base::WeakPtrFactory<FdStringReader> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_FD_STRING_READER_H_
