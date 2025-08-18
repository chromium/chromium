// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_apple.h"

#if defined(WORK_AROUND_CRBUG_40064248)

#include <stdint.h>
#include <sys/socket.h>

namespace net {
namespace {

// A 2-integer struct to give access to the secondary return value, normally
// hidden, that the kernel sets for every system call return.
struct ReturnPair {
  ssize_t primary;      // x0, rax
  uintptr_t secondary;  // x1, rdx
};

// A declaration of `send` with a ReturnPair return value in place of size_t.
// asm("_send") is like an alias: it means that calls to `send_returnpair` will
// actually emit calls to `send`.
extern "C" ReturnPair send_returnpair(int,
                                      void const*,
                                      size_t,
                                      int) asm("_send");

}  // namespace

ssize_t SendAndDetectBogusReturnValue(int const fd,
                                      void const* const buffer,
                                      size_t const size,
                                      int const flags) {
  // TODO(mark): In the future, when a version of macOS with a fix for
  // FB19384824 is published, limit this workaround at runtime to only function
  // on OS versions older than the OS version that contains the fix (such as via
  // base::mac::MacOSVersion and base::ios::IsRunningOnOrLater). On newer OS
  // versions, call `send` directly.
  ReturnPair const rp = send_returnpair(fd, buffer, size, flags);
  if (rp.primary != -1 && rp.secondary != 0) {
    return kSendBogusReturnValueDetected;
  }
  return rp.primary;
}

}  // namespace net

#endif  // WORK_AROUND_CRBUG_40064248
