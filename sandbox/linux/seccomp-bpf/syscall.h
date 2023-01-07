// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_H__
#define SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_H__

#include <signal.h>
#include <stdint.h>

#include "sandbox/linux/system_headers/linux_signal.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// This purely static class can be used to perform system calls with some
// low-level control.
class SANDBOX_EXPORT Syscall {
 public:
  Syscall() = delete;
  Syscall(const Syscall&) = delete;
  Syscall& operator=(const Syscall&) = delete;

  // InvalidCall() invokes Call() with a platform-appropriate syscall
  // number that is guaranteed to not be implemented (i.e., normally
  // returns -ENOSYS).
  // This is primarily meant to be useful for writing sandbox policy
  // unit tests.
  static intptr_t InvalidCall();

  // System calls can take up to six parameters (up to eight on some
  // architectures). Traditionally, glibc
  // implements this property by using variadic argument lists. This works, but
  // confuses tools that behave like Valgrind, because we are nominally passing
  // uninitialized data whenever we call through this function and pass less
  // than the full six arguments.
  // So, instead, we use C++'s template system to achieve a very similar
  // effect. C++ automatically sets the unused parameters to zero for us, and
  // it also does the correct type expansion (e.g. from 32bit to 64bit) where
  // necessary.
  // We have to use C-style cast operators as we want to be able to accept both
  // integer and pointer types.
  template <class T0,
            class T1,
            class T2,
            class T3,
            class T4,
            class T5,
            class T6,
            class T7>
  static inline intptr_t
  Call(int nr, T0 p0, T1 p1, T2 p2, T3 p3, T4 p4, T5 p5, T6 p6, T7 p7) {
    return Call(nr,
                (intptr_t)p0,
                (intptr_t)p1,
                (intptr_t)p2,
                (intptr_t)p3,
                (intptr_t)p4,
                (intptr_t)p5,
                (intptr_t)p6,
                (intptr_t)p7);
  }

  template <class T0,
            class T1,
            class T2,
            class T3,
            class T4,
            class T5,
            class T6>
  static inline intptr_t
  Call(int nr, T0 p0, T1 p1, T2 p2, T3 p3, T4 p4, T5 p5, T6 p6) {
    return Call(nr,
                (intptr_t)p0,
                (intptr_t)p1,
                (intptr_t)p2,
                (intptr_t)p3,
                (intptr_t)p4,
                (intptr_t)p5,
                (intptr_t)p6,
                0);
  }

  template <class T0, class T1, class T2, class T3, class T4, class T5>
  static inline intptr_t
  Call(int nr, T0 p0, T1 p1, T2 p2, T3 p3, T4 p4, T5 p5) {
    return Call(nr,
                (intptr_t)p0,
                (intptr_t)p1,
                (intptr_t)p2,
                (intptr_t)p3,
                (intptr_t)p4,
                (intptr_t)p5,
                0,
                0);
  }

  template <class T0, class T1, class T2, class T3, class T4>
  static inline intptr_t Call(int nr, T0 p0, T1 p1, T2 p2, T3 p3, T4 p4) {
    return Call(nr, p0, p1, p2, p3, p4, 0, 0, 0);
  }

  template <class T0, class T1, class T2, class T3>
  static inline intptr_t Call(int nr, T0 p0, T1 p1, T2 p2, T3 p3) {
    return Call(nr, p0, p1, p2, p3, 0, 0, 0, 0);
  }

  template <class T0, class T1, class T2>
  static inline intptr_t Call(int nr, T0 p0, T1 p1, T2 p2) {
    return Call(nr, p0, p1, p2, 0, 0, 0, 0, 0);
  }

  template <class T0, class T1>
  static inline intptr_t Call(int nr, T0 p0, T1 p1) {
    return Call(nr, p0, p1, 0, 0, 0, 0, 0, 0);
  }

  template <class T0>
  static inline intptr_t Call(int nr, T0 p0) {
    return Call(nr, p0, 0, 0, 0, 0, 0, 0, 0);
  }

  static inline intptr_t Call(int nr) {
    return Call(nr, 0, 0, 0, 0, 0, 0, 0, 0);
  }

  // Set the registers in |ctx| to match what they would be after a system call
  // returning |ret_val|. |ret_val| must follow the Syscall::Call() convention
  // of being -errno on errors.
  static void PutValueInUcontext(intptr_t ret_val, ucontext_t* ctx);

 private:
  // This performs system call |nr| with the arguments p0 to p7 from a constant
  // userland address, which is for instance observable by seccomp-bpf filters.
  // The constant userland address from which these system calls are made will
  // be returned if |nr| is passed as -1.
  // On error, this function will return a value between -1 and -4095 which
  // should be interpreted as -errno.
  static intptr_t Call(int nr,
                       intptr_t p0,
                       intptr_t p1,
                       intptr_t p2,
                       intptr_t p3,
                       intptr_t p4,
                       intptr_t p5,
                       intptr_t p6,
                       intptr_t p7);

#if defined(__mips__)
  // This function basically does on MIPS what SandboxSyscall() is doing on
  // other architectures. However, because of specificity of MIPS regarding
  // handling syscall errors, SandboxSyscall() is made as a wrapper for this
  // function in order for SandboxSyscall() to behave more like on other
  // architectures on places where return value from SandboxSyscall() is used
  // directly (like in most tests).
  // The syscall "nr" is called with arguments that are set in an array on which
  // pointer "args" points to and an information weather there is an error or no
  // is returned to SandboxSyscall() by err_stat.
  static intptr_t SandboxSyscallRaw(int nr,
                                    const intptr_t* args,
                                    intptr_t* err_stat);
#endif  // defined(__mips__)
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SYSCALL_H__
