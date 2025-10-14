// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_APPLE_H_
#define NET_SOCKET_SOCKET_APPLE_H_

#include <Availability.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "build/build_config.h"

// This is a workaround for https://crbug.com/40064248. See also: b/283787255,
// FB12198214, and FB19384824.
//
// In this bug, the `sendto` and `write` system calls, and the `send` wrapper
// around `sendto`, appear to return bogus values under certain conditions. This
// has been observed when writing to established IPv6 (AF_INET6) sockets, both
// TCP (SOCK_STREAM) and UDP (SOCK_DGRAM), following certain network
// reconfigurations on the system. It occurs when bringing up a utun-based VPN,
// when data sent via the socket would become subject to the tunnel. The bug
// occurs on macOS 13.3 22E252 (2023-03-27) and later OS versions, and will be
// fixed in macOS 26.1.
//
// This discussion focuses on `sendto` but the bug is identical for `send`, as
// `send` is a thin C wrapper tail-calling the `sendto` system call; for
// `write`, substitute SYS_write = 4 for SYS_sendto = 133.
//
// ssize_t sendto(int fd, void const* buffer, size_t size, int flags, sockaddr
//                const* address, socklen_t address_size)
//
// `size` contains the number of bytes in `buffer` to be sent via the socket
// whose file descriptor is `fd`.
//
// The ssize_t return value (rv) should be:
//  - rv = -1 for an error, with errno set appropriately; or
//  - 0 ≤ rv ≤ size on success. rv contains the number of bytes accepted by the
//    kernel, conveying the portion of `buffer` that was either sent or queued
//    for sending. This may be equal to `size` if the entire buffer was
//    accepted, or it may be less than `size` if the kernel did not accept the
//    entire buffer (a “short write”).
//
// When the bug occurs, `sendto` appears to return successfully (not -1) but
// reports a bogus value in place of the number of bytes accepted.
//  - On arm64 (including x86_64-on-arm64 via Rosetta translation), the bogus
//    return value is the value of `fd` passed to `sendto`.
//  - On x86_64 (without binary translation), the bogus return value is the
//    system call class (SYSCALL_CLASS_UNIX = 2) and number (SYS_sendto = 133),
//    packed into a single integer: 0x2000085.
//
// The characteristics of the bogus return value impact how easy it is to detect
// the bug’s occurrence with a defensive return value checking technique.
//  - rv > size: The bug has unambiguously occurred. This would mean that the
//    kernel has accepted more data than was provided in `buffer`, which is
//    impossible. This is easy to detect. 0x2000085 (32MB + 133) is almost
//    always larger than the buffer size, so these unambiguous returns occur
//    frequently when the bug occurs on x86_64.
//  - rv ≤ size: This is indistinguishable from a normal successful return. File
//    descriptor numbers are normally small, so these ambiguous return values
//    appear frequently when the bug occurs on arm64.
//
// The mechanics of the bug are specific to the kernel’s architecture, largely
// following each architecture’s standard function call ABI, with an extra
// register dedicated to selecting the system call, and an architectural flag
// bit used to distinguish successful from error returns.
//  - On arm64, the system call number (SYS_sendto = 133) is stored in x16.
//    Arguments are presented in w0 (the low 32 bits of x0) = fd, x1 = buffer,
//    x2 = size, w3 = flags, x4 = address, and w5 = address_size. x0 is used for
//    the return value or error number. cpsr.c (the carry flag of “nzcv”) is set
//    for an error return, and clear for a successful return.
//  - On x86_64, the system call class and number (0x2000085) is stored in eax
//    (the low 32 bits of rax). Arguments are presented in edi = fd, rsi =
//    buffer, rdx = size, ecx = flags, r8 = address, and r9d = address_size. rax
//    is used for the return value or error number. rflags.cf (the carry flag)
//    is set for an error return, and clear for a successful return.
//
// The bug occurs when the EJUSTRETURN path is incorrectly taken in the kernel
// on system call return. xnu source code references are to xnu-11417.121.6,
// which shipped with macOS 15.5 24F74 (2025-05-12):
//  - https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.121.6/bsd/dev/arm/systemcalls.c#L504
//  - https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.121.6/bsd/dev/i386/systemcalls.c#L411
//
// The EJUSTRETURN path exists at this level primarily for the use of the
// `sigreturn` system call, which returns from a user-space signal handler
// function back to the interrupted user thread via the kernel. `sigreturn`
// restores the interrupted thread context, and has no return to its caller
// proper, so EJUSTRETURN exists to suppress the normal register manipulation
// done during any other system call return.
//
// There are also kernel-internal uses of EJUSTRETURN, but aside from
// `sigreturn`, none should “leak” to system call return. Socket code and other
// networking code in the kernel use EJUSTRETURN internally, at that layer
// generally meaning that no further processing should be performed. The bug is
// caused when one of these internal uses of EJUSTRETURN, xnu bsd/net/pf_ioctl.c
// `pf_inet6_hook`, is not handled properly within the networking layer and
// instead propagates from that layer to become `sendto`’s return, improperly
// “leaking” to the system call return level where it takes on a different
// meaning.
//
// When the bug occurs and the EJUSTRETURN path is taken for a return from
// `sendto`, the user-bound return value (uthread->uu_rval[0]) does not make it
// into the register state to be restored on user return (x0 via ss64->x[0], rax
// via regs->rax), leaving the return value register’s previous contents from
// system call entry intact. x0 will still contain the file descriptor number,
// and rax will still contain the system call selector. The carry flag will have
// been optimistically cleared, so the bug’s occurrence is always observed as a
// successful return in the user program.
//
// To provide more robust detection of even the ambiguous case, this workaround
// leverages the fact that all successful system call returns set a secondary
// return register in addition to the primary return value in x0 and rax. The
// secondary return registers are x1 and rdx, identical to each architecture’s
// ABI for a return of a struct containing 2 integers. For the vast majority of
// system calls (only `fork` and `pipe` are exceptions), the secondary return
// register is cleared. Thus, if the secondary return register is nonzero on
// system call entry, and it remains nonzero on system call return, it can be
// taken as a signal that the bug occurred unambiguously.
//
// By architecture:
//  - On arm64, x1 = buffer. x1 can be consulted to detect the bug unambiguously
//    as long as buffer != nullptr. If buffer == nullptr and size > 0, the call
//    would have resulted in an error return (EFAULT) so the bug would not have
//    been observed.
//  - On x86_64, rdx = size. rdx can be consulted to detect the bug
//    unambiguously as long as size != 0.
//
// For slightly different reasons on each architecture, it is possible for the
// secondary return value mechanism to fail to detect the bug’s occurrence when
// size == 0. The bug cannot occur for a `sendto` on a TCP socket with size ==
// 0, because sending 0 bytes via TCP is a no-op, and an early-return path is
// taken in the kernel without the bogus return value appearing. Thus, for any
// TCP socket, the secondary return value alone provides robust and unambiguous
// detection of the bug’s occurrence.
//
// A 0-byte `sendto` on a UDP socket is valid (it sends or queues a packet with
// no data payload beyond the UDP header), so for a 0-byte `sendto`, the
// secondary return register does not indicate the bug’s occurrence. Leveraging
// the fact that a 0-byte successful `sendto` can only validly return 0, the
// primary return value being nonzero can provide unambiguous detection of the
// bug’s occurrence on x86_64. On arm64, there’s a small amount of potential
// confusion in that w0 = fd, and 0 is valid as a file descriptor. Fortunately,
// as a file descriptor, 0 is STDIN_FILENO, and this code is not likely to
// manipulate a socket on the standard input stream, and even less likely to
// `sendto` via an input stream (although this is by convention, not strict
// requirement).
//
// The workaround is implemented in a bug-detecting wrapper around `sendto`.
// Substitute the SendtoAndDetectBogusReturnValue wrapper for a call to
// `sendto`, or SendAndDetectBogusReturnValue wrapper for a call to `send`, and
// when the bug occurs and is detected, its return value will be
// kSendBogusReturnValueDetected. In all other respects, the wrappers behave
// identically to `sendto` and `send`.
//
// Assuming that `send` tail-calls `sendto`, and a successful `sendto` returns
// immediately from the system call to its caller without modifying any
// registers, this detection mechanism will be valid. These assumptions hold
// empirically, as well as through a read of all of the source code on both the
// user and kernel sides of the system call boundary, at all relevant OS
// versions. However, there’s no guarantee that it must hold into the future,
// and it’s possible that a future OS version might invalidate these
// assumptions. In that case, this technique of detecting the bug’s occurrence
// via x1 and rdx might be jeopardized. This workaround is only attempted on
// OS versions where it’s known to be necessary.
//
// Note that these assumptions are not valid for an error return from the system
// call, and the secondary return register is not set during an error return
// either, so the bug-detecting wrapper takes care to only attempt detection
// during apparent successful returns.

// The assumptions above aren’t valid for certain sanitizers. Under Address
// Sanitizer, some system calls are intercepted via interposition, so
// libclang_rt.asan_osx_dynamic.dylib’s `wrap_sendto`, which does pre- and
// post-processing around a call to `sendto`, will appear when the program
// expects to call `sendto` directly. ASan’s interceptors exist between the
// wrapping that this workaround implements and the underlying system call; the
// basis of implementation for the ones relevant to this workaround is llvm
// compiler-rt/lib/sanitizer_common/sanitizer_common_interceptors.inc. Unaware
// of the normally hidden secondary system call return register, ASan’s
// interceptors may clobber it before this workaround’s wrapper has an
// opportunity to examine it. Address Sanitizer and Thread Sanitizer’s runtime
// libraries both expose `wrap_sendto`, so don’t attempt this workaround in ASan
// or TSan builds.
//
// FB19384824 will be fixed in macOS 26.1 and iOS 26.1 (the fix is present as of
// 26.1b2). When the minimum runtime OS version is at or beyond this, disable
// the workaround entirely at compile time.
#if !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) &&          \
    ((BUILDFLAG(IS_MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 26'01'00) || \
     (BUILDFLAG(IS_IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED < 26'01'00))
#define WORK_AROUND_CRBUG_40064248 1

namespace net {

// A return value used to signal the bug’s occurrence in-band. This must be
// negative to avoid being confused with any possible successful return value,
// and it must not be -1 to avoid being confused with a normal errno-setting
// error return. In-band signaling makes things easier for callers, because
// `send` and `sendto` can be swapped out easily in favor of their wrappers,
// which can be used equally well with HANDLE_EINTR as appropriate.
inline constexpr ssize_t kSendBogusReturnValueDetected = -2;
static_assert(kSendBogusReturnValueDetected < 0 &&
              kSendBogusReturnValueDetected != -1);

// Wrap `sendto`, returning kSendBogusReturnValueDetected when the bug’s
// occurrence is detected.
ssize_t SendtoAndDetectBogusReturnValue(int fd,
                                        void const* buffer,
                                        size_t size,
                                        int flags,
                                        sockaddr const* address,
                                        socklen_t address_size);

// `send` is the same as `sendto` with the final two arguments zeroed.
inline ssize_t SendAndDetectBogusReturnValue(int const fd,
                                             void const* const buffer,
                                             size_t const size,
                                             int const flags) {
  return SendtoAndDetectBogusReturnValue(fd, buffer, size, flags, nullptr, 0);
}

}  // namespace net

#endif  // !ADDRESS_SANITIZER && !THREAD_SANITIZER && ((MAC && DT < 26.1) ||
        // (IOS && DT < 26.1))

#endif  // NET_SOCKET_SOCKET_APPLE_H_
