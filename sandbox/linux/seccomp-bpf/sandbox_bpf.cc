// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/bpf_dsl/syscall_set.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/system_headers/linux_filter.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/sandbox_buildflags.h"

namespace sandbox {

namespace {

// Check if the kernel supports seccomp-filter (a.k.a. seccomp mode 2) via
// prctl().
bool KernelSupportsSeccompBPF() {
  errno = 0;
  const int rv = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, nullptr);

  if (rv == -1 && EFAULT == errno) {
    return true;
  }
  return false;
}

// LG introduced a buggy syscall, sys_set_media_ext, with the same number as
// seccomp. Return true if the current kernel has this buggy syscall.
//
// We want this to work with upcoming versions of seccomp, so we pass bogus
// flags that are unlikely to ever be used by the kernel. A normal kernel would
// return -EINVAL, but a buggy LG kernel would return 1.
bool KernelHasLGBug() {
#if BUILDFLAG(IS_ANDROID)
  // sys_set_media will see this as NULL, which should be a safe (non-crashing)
  // way to invoke it. A genuine seccomp syscall will see it as
  // SECCOMP_SET_MODE_STRICT.
  const unsigned int operation = 0;
  // Chosen by fair dice roll. Guaranteed to be random.
  const unsigned int flags = 0xf7a46a5c;
  const int rv = sys_seccomp(operation, flags, nullptr);
  // A genuine kernel would return -EINVAL (which would set rv to -1 and errno
  // to EINVAL), or at the very least return some kind of error (which would
  // set rv to -1). Any other behavior indicates that whatever code received
  // our syscall was not the real seccomp.
  if (rv != -1) {
    return true;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  return false;
}

bool KernelSupportsSeccompFlags(unsigned int flags) {
  if (KernelHasLGBug()) {
    return false;
  }

  errno = 0;
  const int rv = sys_seccomp(SECCOMP_SET_MODE_FILTER, flags, nullptr);

  if (rv == -1 && errno == EFAULT) {
    return true;
  }

  DCHECK_EQ(-1, rv);
  DCHECK(ENOSYS == errno || EINVAL == errno);
  return false;
}

// Check if the kernel supports seccomp-filter via the seccomp system call
// and the TSYNC feature to enable seccomp on all threads.
bool KernelSupportsSeccompTsync() {
  return KernelSupportsSeccompFlags(SECCOMP_FILTER_FLAG_TSYNC);
}

#if BUILDFLAG(DISABLE_SECCOMP_SSBD)
// Check if the kernel supports seccomp-filter via the seccomp system call and
// without spec flaw mitigation.
bool KernelSupportSeccompSpecAllow() {
  return KernelSupportsSeccompFlags(SECCOMP_FILTER_FLAG_SPEC_ALLOW);
}
#endif

uint64_t EscapePC() {
  intptr_t rv = Syscall::Call(-1);
  if (rv == -1 && errno == ENOSYS) {
    return 0;
  }
  return static_cast<uint64_t>(static_cast<uintptr_t>(rv));
}

intptr_t SandboxPanicTrap(const struct arch_seccomp_data&, void* aux) {
  SANDBOX_DIE(static_cast<const char*>(aux));
}

bpf_dsl::ResultExpr SandboxPanic(const char* error) {
  return bpf_dsl::Trap(SandboxPanicTrap, error);
}

}  // namespace

SandboxBPF::SandboxBPF(std::unique_ptr<bpf_dsl::Policy> policy)
    : proc_fd_(), sandbox_has_started_(false), policy_(std::move(policy)) {}

SandboxBPF::~SandboxBPF() {
}

// static
bool SandboxBPF::SupportsSeccompSandbox(SeccompLevel level) {
  switch (level) {
    case SeccompLevel::SINGLE_THREADED:
      return KernelSupportsSeccompBPF();
    case SeccompLevel::MULTI_THREADED:
      return KernelSupportsSeccompTsync();
  }
  NOTREACHED();
}

bool SandboxBPF::StartSandbox(SeccompLevel seccomp_level, bool enable_ibpb) {
  DCHECK(policy_);
  CHECK(seccomp_level == SeccompLevel::SINGLE_THREADED ||
        seccomp_level == SeccompLevel::MULTI_THREADED);

  if (sandbox_has_started_) {
    SANDBOX_DIE(
        "Cannot repeatedly start sandbox. Create a separate Sandbox "
        "object instead.");
  }

  if (!proc_fd_.is_valid()) {
    SetProcFd(ProcUtil::OpenProc());
  }

  const bool supports_tsync = KernelSupportsSeccompTsync();

  if (seccomp_level == SeccompLevel::SINGLE_THREADED) {
    // Wait for /proc/self/task/ to update if needed and assert the
    // process is single threaded.
    ThreadHelpers::AssertSingleThreaded(proc_fd_.get());
  } else if (seccomp_level == SeccompLevel::MULTI_THREADED) {
    if (!supports_tsync) {
      SANDBOX_DIE("Cannot start sandbox; kernel does not support synchronizing "
                  "filters for a threadgroup");
    }
  }

  // We no longer need access to any files in /proc. We want to do this
  // before installing the filters, just in case that our policy denies
  // close().
  if (proc_fd_.is_valid()) {
    proc_fd_.reset();
  }

  // Install the filters.
  InstallFilter(seccomp_level == SeccompLevel::MULTI_THREADED, enable_ibpb);

  return true;
}

void SandboxBPF::SetProcFd(base::ScopedFD proc_fd) {
  if (proc_fd_.get() != proc_fd.get())
    proc_fd_ = std::move(proc_fd);
}

// static
bool SandboxBPF::IsValidSyscallNumber(int sysnum) {
  return SyscallSet::IsValid(sysnum);
}

// static
bool SandboxBPF::IsRequiredForUnsafeTrap(int sysno) {
  return bpf_dsl::PolicyCompiler::IsRequiredForUnsafeTrap(sysno);
}

// static
intptr_t SandboxBPF::ForwardSyscall(const struct arch_seccomp_data& args) {
  return Syscall::Call(
      args.nr, static_cast<intptr_t>(args.args[0]),
      static_cast<intptr_t>(args.args[1]), static_cast<intptr_t>(args.args[2]),
      static_cast<intptr_t>(args.args[3]), static_cast<intptr_t>(args.args[4]),
      static_cast<intptr_t>(args.args[5]));
}

CodeGen::Program SandboxBPF::AssembleFilter() {
  DCHECK(policy_);

  bpf_dsl::PolicyCompiler compiler(policy_.get(), Trap::Registry());
  if (Trap::SandboxDebuggingAllowedByUser()) {
    compiler.DangerousSetEscapePC(EscapePC());
  }
  compiler.SetPanicFunc(SandboxPanic);
  return compiler.Compile();
}

void SandboxBPF::InstallFilter(bool must_sync_threads, bool enable_ibpb) {
  // We want to be very careful in not imposing any requirements on the
  // policies that are set with SetSandboxPolicy(). This means, as soon as
  // the sandbox is active, we shouldn't be relying on libraries that could
  // be making system calls. This, for example, means we should avoid
  // using the heap and we should avoid using STL functions.
  // Temporarily copy the contents of the "program" vector into a
  // stack-allocated array; and then explicitly destroy that object.
  // This makes sure we don't ex- or implicitly call new/delete after we
  // installed the BPF filter program in the kernel. Depending on the
  // system memory allocator that is in effect, these operators can result
  // in system calls to things like munmap() or brk().
  CodeGen::Program program = AssembleFilter();

// Silence clang's warning about allocating on the stack because we have no
// other choice.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-extension"
  struct sock_filter bpf[program.size()];
#pragma clang diagnostic pop
  const struct sock_fprog prog = {static_cast<unsigned short>(program.size()),
                                  bpf};
  memcpy(bpf, &program[0], sizeof(bpf));
  CodeGen::Program().swap(program);  // vector swap trick

  // Make an attempt to release memory that is no longer needed here, rather
  // than in the destructor. Try to avoid as much as possible to presume of
  // what will be possible to do in the new (sandboxed) execution environment.
  policy_.reset();

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    SANDBOX_DIE("Kernel refuses to enable no-new-privs");
  }

  // Install BPF filter program. If the thread state indicates that tsync is
  // necessary or SECCOMP_FILTER_FLAG_SPEC_ALLOW is supported, then the kernel
  // has the seccomp system call. Otherwise, fall back on prctl, which requires
  // the process to be single-threaded.
  unsigned int seccomp_filter_flags = 0;
  if (must_sync_threads) {
    seccomp_filter_flags |= SECCOMP_FILTER_FLAG_TSYNC;
#if BUILDFLAG(DISABLE_SECCOMP_SSBD)
    // Seccomp will disable indirect branch speculation and speculative store
    // bypass simultaneously. To only opt-out SSBD, following steps are needed
    // 1. Disable IBSpec with prctl
    // 2. Call seccomp with SECCOMP_FILTER_FLAG_SPEC_ALLOW
    // As prctl provide a per thread control of the speculation feature, only
    // opt-out SSBD when process is single-threaded and tsync is not necessary.
  } else if (KernelSupportSeccompSpecAllow()) {
    seccomp_filter_flags |= SECCOMP_FILTER_FLAG_SPEC_ALLOW;
    if (enable_ibpb) {
      DisableIBSpec();
    }
#endif
  } else {
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
      SANDBOX_DIE("Kernel refuses to turn on BPF filters");
    }
    sandbox_has_started_ = true;
    return;
  }

  int rv = sys_seccomp(SECCOMP_SET_MODE_FILTER, seccomp_filter_flags, &prog);
  if (rv) {
    SANDBOX_DIE("Kernel refuses to turn on BPF filters");
  }
  sandbox_has_started_ = true;
}

void SandboxBPF::DisableIBSpec() {
  // Test if the per-task control of the mitigation is available. If
  // PR_SPEC_PRCTL is set, then the per-task control of the mitigation is
  // available. If not set, prctl(PR_SET_SPECULATION_CTRL) for the speculation
  // misfeature will fail.
  const int rv =
      prctl(PR_GET_SPECULATION_CTRL, PR_SPEC_INDIRECT_BRANCH, 0, 0, 0);
  // Kernel control of the speculation misfeature is not supported or the
  // misfeature is already force disabled.
  if (rv < 0 || (rv & PR_SPEC_FORCE_DISABLE)) {
    return;
  }

  if (!(rv & PR_SPEC_PRCTL)) {
    DVLOG(1) << "Indirect branch speculation can not be controled by prctl. "
             << rv;
    return;
  }

  if (prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_INDIRECT_BRANCH,
            PR_SPEC_FORCE_DISABLE, 0, 0)) {
    PLOG(ERROR) << "Kernel failed to force disable indirect branch speculation";
  }
}

}  // namespace sandbox
