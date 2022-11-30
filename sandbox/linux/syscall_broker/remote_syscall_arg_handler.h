// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_REMOTE_SYSCALL_ARG_HANDLER_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_REMOTE_SYSCALL_ARG_HANDLER_H_

#include <unistd.h>

#include "base/containers/span.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace syscall_broker {

enum class RemoteProcessIOResult {
  kSuccess,
  kRemoteExited,
  kExceededPathMax,
  kRemoteMemoryInvalid,
  kUnknownError
};

// Writes |data| at |remote_addr| in |pid|'s address space. Returns the
// appropriate result.
SANDBOX_EXPORT RemoteProcessIOResult WriteRemoteData(pid_t pid,
                                                     uintptr_t remote_addr,
                                                     size_t remote_size,
                                                     base::span<char> data);

// Reads a filepath from |remote_addr| (which points into process |pid|'s memory
// space) into |*out_str|. Returns the appropriate result.
// Safety checks should occur before usage of any system call arguments read
// from a remote address space, so callers should use RemoteSyscallFilepathArgs
// instead of calling this directly.
SANDBOX_EXPORT RemoteProcessIOResult
ReadFilePathFromRemoteProcess(pid_t pid,
                              const void* remote_addr,
                              std::string* out_str);

namespace internal {
// The number of bytes we read from a remote process at a time when reading a
// remote filepath, to avoid reading PATH_MAX bytes every time.
const size_t kNumBytesPerChunk = 256;

// Calculates the number of bytes left in a page for a particular address.
uintptr_t NumBytesLeftInPage(uintptr_t addr);
}  // namespace internal
}  // namespace syscall_broker
}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_REMOTE_SYSCALL_ARG_HANDLER_H_
