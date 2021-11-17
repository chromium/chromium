// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// base::AddressIsReadable() probes an address to see whether it is readable,
// without faulting.

#include "absl/debugging/internal/address_is_readable.h"

#if !defined(__linux__) || defined(__ANDROID__)

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace debugging_internal {

// On platforms other than Linux, just return true.
bool AddressIsReadable(const void* /* addr */) { return true; }

}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl

#else

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>

#include "absl/base/internal/errno_saver.h"
#include "absl/base/internal/raw_logging.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace debugging_internal {

bool AddressIsReadable(const void *addr) {
  int fd = 0;
  absl::base_internal::ErrnoSaver errno_saver;
  for (int j = 0; j < 2; j++) {
    // Here we probe with some syscall which
    // - accepts a one-byte region of user memory as input
    // - tests for EFAULT before other validation
    // - has no problematic side-effects
    //
    // connect(2) works for this.  It copies the address into kernel
    // memory before any validation beyond requiring an open fd.
    // But a one byte address is never valid (sa_family is two bytes),
    // so the call cannot succeed and change any state.
    //
    // This strategy depends on Linux implementation details,
    // so we rely on the test to alert us if it stops working.
    //
    // Some discarded past approaches:
    // - msync() doesn't reject PROT_NONE regions
    // - write() on /dev/null doesn't return EFAULT
    // - write() on a pipe requires creating it and draining the writes
    //
    // Use syscall(SYS_connect, ...) instead of connect() to prevent ASAN
    // and other checkers from complaining about accesses to arbitrary memory.
    do {
      ABSL_RAW_CHECK(syscall(SYS_connect, fd, addr, 1) == -1,
                     "should never succeed");
    } while (errno == EINTR);
    if (errno == EFAULT) return false;
    if (errno == EBADF) {
      if (j != 0) {
        // Unclear what happened.
        ABSL_RAW_LOG(ERROR, "unexpected EBADF on fd %d", fd);
        return false;
      }
      // fd 0 must have been closed. Try opening it again.
      // Note: we shouldn't leak too many file descriptors here, since we expect
      // to get fd==0 reopened.
      fd = open("/dev/null", O_RDONLY);
      if (fd == -1) {
        ABSL_RAW_LOG(ERROR, "can't open /dev/null");
        return false;
      }
    } else {
      // probably EINVAL or ENOTSOCK; we got past EFAULT validation.
      return true;
    }
  }
  ABSL_RAW_CHECK(false, "unreachable");
  return false;
}

}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif
