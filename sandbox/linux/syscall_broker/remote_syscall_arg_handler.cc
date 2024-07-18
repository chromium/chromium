// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/syscall_broker/remote_syscall_arg_handler.h"

#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#if defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#endif

namespace sandbox {
namespace syscall_broker {

RemoteProcessIOResult WriteRemoteData(pid_t pid,
                                      uintptr_t remote_addr,
                                      size_t remote_size,
                                      base::span<char> data) {
  CHECK_GE(remote_size, data.size());

  base::span<char> remote_span(reinterpret_cast<char*>(remote_addr),
                               remote_size);
  struct iovec local_iov = {};
  struct iovec remote_iov = {};

  while (!data.empty()) {
    local_iov.iov_base = data.data();
    local_iov.iov_len = data.size();
    remote_iov.iov_base = remote_span.data();
    remote_iov.iov_len = data.size();

    ssize_t bytes_written = syscall(__NR_process_vm_writev, pid, &local_iov,
                                    1ul, &remote_iov, 1ul, 0ul);
    if (bytes_written < 0) {
      if (errno == EFAULT)
        return RemoteProcessIOResult::kRemoteMemoryInvalid;
      if (errno == ESRCH)
        return RemoteProcessIOResult::kRemoteExited;
      PLOG(ERROR)
          << "process_vm_writev() failed with unknown error code! Write to pid "
          << pid << " at remote address " << remote_iov.iov_base
          << " of length " << data.size() << ". ";
      return RemoteProcessIOResult::kUnknownError;
    }

    remote_span = remote_span.subspan(bytes_written);
    data = data.subspan(bytes_written);
  }

  return RemoteProcessIOResult::kSuccess;
}

RemoteProcessIOResult ReadFilePathFromRemoteProcess(pid_t pid,
                                                    const void* remote_addr,
                                                    std::string* out_str) {
  // Most pathnames will be small so avoid copying PATH_MAX bytes every time,
  // by reading in chunks and checking if the the string ends within the
  // chunk.
  char buffer[PATH_MAX];
  base::span<char> buffer_span(buffer);

  struct iovec local_iov = {};
  struct iovec remote_iov = {};

  uintptr_t remote_ptr = reinterpret_cast<uintptr_t>(remote_addr);

  for (;;) {
    uintptr_t bytes_left_in_page = internal::NumBytesLeftInPage(remote_ptr);

    // Read the minimum of the chunk size, remaining local buffer size, and
    // the number of bytes left in the remote page.
    size_t bytes_to_read = std::min(
        {internal::kNumBytesPerChunk, buffer_span.size(), bytes_left_in_page});

    // Set up the iovecs.
    local_iov.iov_base = buffer_span.data();
    local_iov.iov_len = bytes_to_read;

    remote_iov.iov_base = reinterpret_cast<void*>(remote_ptr);
    remote_iov.iov_len = bytes_to_read;

    // The arguments below must include the ul suffix since they need to be
    // 64-bit values, but syscall() takes varargs and doesn't know to promote
    // them from 32-bit to 64-bit.
    ssize_t bytes_read = syscall(__NR_process_vm_readv, pid, &local_iov, 1ul,
                                 &remote_iov, 1ul, 0ul);
    if (bytes_read < 0) {
      if (errno == EFAULT)
        return RemoteProcessIOResult::kRemoteMemoryInvalid;
      if (errno == ESRCH)
        return RemoteProcessIOResult::kRemoteExited;
      PLOG(ERROR)
          << "process_vm_readv() failed with unknown error code! Read from pid "
          << pid << " at remote address " << remote_iov.iov_base
          << " of length " << bytes_to_read << ". ";
      return RemoteProcessIOResult::kUnknownError;
    }

    // We successfully performed a read.
#if defined(MEMORY_SANITIZER)
    // Msan does not hook syscall(__NR_process_vm_readv, ...)
    __msan_unpoison(local_iov.iov_base, bytes_read);
#endif
    remote_ptr += bytes_read;
    buffer_span = buffer_span.subspan(bytes_read);

    // Check for null byte.
    char* null_byte_ptr =
        static_cast<char*>(memchr(local_iov.iov_base, '\0', bytes_read));
    if (null_byte_ptr) {
      *out_str = std::string(buffer, null_byte_ptr);
      return RemoteProcessIOResult::kSuccess;
    }

    if (buffer_span.empty()) {
      // If we haven't found a null byte yet and our available buffer space is
      // empty, stop.
      LOG(ERROR) << "Read PATH_MAX bytes in sandboxed process and did not find "
                    "expected null byte.";
      return RemoteProcessIOResult::kExceededPathMax;
    }
  }
}

namespace internal {
uintptr_t NumBytesLeftInPage(uintptr_t addr) {
  const uintptr_t page_end = base::bits::AlignUp(addr + 1, base::GetPageSize());
  return page_end - addr;
}
}  // namespace internal
}  // namespace syscall_broker
}  // namespace sandbox
