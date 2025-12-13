// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_apple.h"

#if defined(WORK_AROUND_CRBUG_40064248)

#include <stdint.h>
#include <sys/syscall.h>

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#elif BUILDFLAG(IS_IOS)
#include "base/system/sys_info.h"
#endif

namespace net {
namespace {

bool OSVersionIsAffected() {
#if BUILDFLAG(IS_MAC)
  // FB19384824 was introduced in macOS 13.3 and will be fixed in macOS 26.1.
  const int os_version = base::mac::MacOSVersion();
  return os_version >= 13'03'00 && os_version < 26'01'00;
#elif BUILDFLAG(IS_IOS)
  // These iOS version numbers that correspond to the macOS version numbers
  // above.
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  const int os_version = major * 1'00'00 + minor * 1'00 + bugfix;
  return os_version >= 16'03'00 && os_version < 26'01'00;
#endif
}

// A 2-integer struct to give access to the secondary return value, normally
// hidden, that the kernel sets for every system call return.
struct ReturnPair {
  ssize_t primary;      // x0, rax
  uintptr_t secondary;  // x1, rdx
};

// A declaration of `sendto` with a ReturnPair return value in place of ssize_t.
// asm("_sendto") is like an alias: it means that calls to `sendto_returnpair`
// will actually emit calls to `sendto`.
extern "C" ReturnPair sendto_returnpair(int,
                                        void const*,
                                        size_t,
                                        int,
                                        sockaddr const*,
                                        socklen_t) asm("_sendto");

}  // namespace

ssize_t SendtoAndDetectBogusReturnValue(int const fd,
                                        void const* const buffer,
                                        size_t const size,
                                        int const flags,
                                        sockaddr const* const address,
                                        socklen_t const address_size) {
  static const bool os_version_is_affected = OSVersionIsAffected();
  if (!os_version_is_affected) {
    return sendto(fd, buffer, size, flags, address, address_size);
  }

  ReturnPair const rp =
      sendto_returnpair(fd, buffer, size, flags, address, address_size);

#if defined(ARCH_CPU_ARM64)
  uintptr_t const param_shared_with_secondary =
      reinterpret_cast<uintptr_t>(buffer);
  constexpr ssize_t kSuspiciousRv_x86_64 = 0;
#elif defined(ARCH_CPU_X86_64)
  uintptr_t const param_shared_with_secondary = size;
  constexpr ssize_t kSuspiciousRv_x86_64 = (2 << 24) | SYS_sendto;  // 0x2000085
#endif  // ARCH_CPU_*

  // When the bug occurs, the apparent (primary) return value will not be
  // negative, so check rp.primary first.
  //
  // For a successful return when the bug hasn’t occurred, rp.secondary will be
  // set to 0. If rp.secondary is not 0 on a successful return, the bug has
  // definitely occurred.
  //
  // It’s possible that rp.secondary will be 0 on a successful return even when
  // the bug has occurred, if the register shared with rp.secondary,
  // param_shared_with_secondary, contained 0 on system call entry. In that
  // case, `size` must be 0. (arm64: param_shared_with_secondary is `buffer`,
  // and the use of a null pointer here can only be tolerated if `size` is 0;
  // x86_64: param_shared_with_secondary is `size` itself.) The bug can’t occur
  // for a meaningless 0-byte TCP send but it can occur for a meaningful 0-byte
  // UDP send. Since `size` is 0, the bug’s occurrence can be detected by
  // comparing rp.primary to the known suspicious return value.
  //
  // The suspicious return value is `fd` on arm64 kernels and
  // kSuspiciousRv_x86_64 on x86_64 kernels. Since x86_64 user code can run atop
  // an arm64 kernel via Rosetta binary translation, check rp.primary against
  // `fd` regardless of architecture.
  //
  // Since the suspicious return value is `fd` on arm64 kernels, it’s not
  // possible to detect the bug’s occurrence with a 0-byte UDP send to file
  // descriptor 0. But this isn’t expected to ever occur practically, as 0 is
  // STDIN_FILENO, and this code isn’t likely to operate on standard streams,
  // and is even less likely to attempt to send to an input stream.
  if (rp.primary != -1 &&
      (rp.secondary != 0 ||
       (param_shared_with_secondary == 0 &&
        ((rp.primary == fd && fd != 0) ||
         (kSuspiciousRv_x86_64 != 0 && rp.primary == kSuspiciousRv_x86_64))))) {
    return kSendBogusReturnValueDetected;
  }

  return rp.primary;
}

}  // namespace net

#endif  // WORK_AROUND_CRBUG_40064248
