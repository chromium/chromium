// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SIGSYS_HANDLERS_H_
#define SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SIGSYS_HANDLERS_H_

#include <stdint.h>

#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/sandbox_export.h"

// The handlers are suitable for use in Trap() error codes. They are
// guaranteed to be async-signal safe.
// See sandbox/linux/seccomp-bpf/trap.h to see how they work.

struct arch_seccomp_data;

namespace sandbox {

// This handler will crash the currently running process. The crashing address
// will be the number of the current system call, extracted from |args|.
// This handler will also print to stderr the number of the crashing syscall.
SANDBOX_EXPORT intptr_t CrashSIGSYS_Handler(const arch_seccomp_data& args,
                                            void* aux);

// The following seven handlers are suitable to report failures for specific
// system calls with additional information.

// The crashing address will be (clone_flags & 0xFFFFFF), where clone_flags is
// the clone(2) argument, extracted from |args|.
SANDBOX_EXPORT intptr_t SIGSYSCloneFailure(const arch_seccomp_data& args,
                                           void* aux);
// The crashing address will be (option & 0xFFF), where option is the prctl(2)
// argument.
SANDBOX_EXPORT intptr_t SIGSYSPrctlFailure(const arch_seccomp_data& args,
                                           void* aux);
// The crashing address will be request & 0xFFFF, where request is the ioctl(2)
// argument.
SANDBOX_EXPORT intptr_t SIGSYSIoctlFailure(const arch_seccomp_data& args,
                                           void* aux);
// The crashing address will be (pid & 0xFFF), where pid is the first
// argument (and can be a tid).
SANDBOX_EXPORT intptr_t SIGSYSKillFailure(const arch_seccomp_data& args,
                                          void* aux);
// The crashing address will be (op & 0xFFF), where op is the second
// argument.
SANDBOX_EXPORT intptr_t SIGSYSFutexFailure(const arch_seccomp_data& args,
                                           void* aux);
// The crashing address will be (op & 0xFFF), where op is the second
// argument.
SANDBOX_EXPORT intptr_t SIGSYSPtraceFailure(const arch_seccomp_data& args,
                                            void* aux);
// The crashing address will be ((protocol & 0x1f) << 10) | ((type & 0xf) << 6)
// | (domain & 0x3f). This is 5 bits for protocol (from /etc/protocols), 4 bits
// for type (e.g. SOCK_STREAM, SOCK_RAW, SOCK_PACKET), and 6 bits for domain
// (e.g. AF_UNIX, AF_INET).
SANDBOX_EXPORT intptr_t
SIGSYSSocketFailure(const struct arch_seccomp_data& args, void* aux);
// The crashing address will be ((optname & 0xfful) << 9) | (level & 0x1ff).
// This is 7 bits for optname (e.g. SO_REUSEADDR) and 9 bits for level (from
// /etc/protocols).
SANDBOX_EXPORT intptr_t
SIGSYSSockoptFailure(const struct arch_seccomp_data& args, void* aux);

// If the syscall is not being called on the current tid, crashes in the same
// way as CrashSIGSYS_Handler.  Otherwise, returns the result of calling the
// syscall with the pid argument set to 0 (which for these calls means the
// current thread).  The following syscalls are supported:
//
// sched_getaffinity(), sched_getattr(), sched_getparam(), sched_getscheduler(),
// sched_rr_get_interval(), sched_setaffinity(), sched_setattr(),
// sched_setparam(), sched_setscheduler()
SANDBOX_EXPORT intptr_t SIGSYSSchedHandler(const arch_seccomp_data& args,
                                           void* aux);
// If the fstatat() syscall is functionally equivalent to an fstat() syscall,
// then rewrite the syscall to the equivalent fstat() syscall which can be
// adequately sandboxed.
// If the fstatat() is not functionally equivalent to an fstat() syscall, we
// fail with -fs_denied_errno.
// If the syscall is not an fstatat() at all, crash in the same way as
// CrashSIGSYS_Handler.
// This is necessary because glibc and musl have started rewriting fstat(fd,
// stat_buf) as fstatat(fd, "", stat_buf, AT_EMPTY_PATH). We rewrite the latter
// back to the former, which is actually sandboxable.
SANDBOX_EXPORT intptr_t
SIGSYSFstatatHandler(const struct arch_seccomp_data& args,
                     void* fs_denied_errno);

// Variants of the above functions for use with bpf_dsl.
SANDBOX_EXPORT bpf_dsl::ResultExpr CrashSIGSYS();
SANDBOX_EXPORT bpf_dsl::ResultExpr CrashSIGSYSClone();
SANDBOX_EXPORT bpf_dsl::ResultExpr CrashSIGSYSPrctl();
SANDBOX_EXPORT bpf_dsl::ResultExpr CrashSIGSYSIoctl();
SANDBOX_EXPORT bpf_dsl::ResultExpr CrashSIGSYSKill();
SANDBOX_EXPORT bpf_dsl::ResultExpr CrashSIGSYSFutex();
SANDBOX_EXPORT bpf_dsl::ResultExpr CrashSIGSYSPtrace();
SANDBOX_EXPORT bpf_dsl::ResultExpr CrashSIGSYSSocket();
SANDBOX_EXPORT bpf_dsl::ResultExpr CrashSIGSYSSockopt();
SANDBOX_EXPORT bpf_dsl::ResultExpr RewriteSchedSIGSYS();
SANDBOX_EXPORT bpf_dsl::ResultExpr RewriteFstatatSIGSYS(int fs_denied_errno);

#if defined(__NR_socketcall)
// True if the kernel supports direct socket-API syscalls like socket(2) and
// bind(2). Older x86 kernels only supported socketcall(2), which can't be
// filtered with seccomp. Newer x86 kernels (>=4.3) also support the direct
// syscalls, but unfortunately there's no way to force the libc to use these
// syscalls (bionic in particular does not support this), so this policy will
// rewrite socketcall(2) to the filterable direct syscalls if supported.
SANDBOX_EXPORT bool CanRewriteSocketcall();

// This rewrites a socketcall(2) call to the appropriate direct sockets-API
// syscall like socket(2), which are filterable with seccomp.
SANDBOX_EXPORT intptr_t SIGSYSSocketcallHandler(const arch_seccomp_data& args,
                                                void* aux);
// This should only be used if CanRewriteSocketcall() is true.
SANDBOX_EXPORT bpf_dsl::ResultExpr RewriteSocketcallSIGSYS();
#endif  // defined(__NR_socketcall)

// Allocates a crash key so that Seccomp information can be recorded.
void AllocateCrashKeys();

// Following four functions return substrings of error messages used
// in the above four functions. They are useful in death tests.
SANDBOX_EXPORT const char* GetErrorMessageContentForTests();
SANDBOX_EXPORT const char* GetCloneErrorMessageContentForTests();
SANDBOX_EXPORT const char* GetPrctlErrorMessageContentForTests();
SANDBOX_EXPORT const char* GetIoctlErrorMessageContentForTests();
SANDBOX_EXPORT const char* GetKillErrorMessageContentForTests();
SANDBOX_EXPORT const char* GetFutexErrorMessageContentForTests();
SANDBOX_EXPORT const char* GetPtraceErrorMessageContentForTests();
SANDBOX_EXPORT const char* GetSocketErrorMessageContentForTests();
SANDBOX_EXPORT const char* GetSockoptErrorMessageContentForTests();

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SECCOMP_BPF_HELPERS_SIGSYS_HANDLERS_H_
