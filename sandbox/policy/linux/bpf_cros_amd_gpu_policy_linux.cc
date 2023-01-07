// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_cros_amd_gpu_policy_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/kcmp.h>
#include <sys/socket.h>

// Some arch's (arm64 for instance) unistd.h don't pull in symbols used here
// unless these are defined.
#define __ARCH_WANT_SYSCALL_NO_AT
#define __ARCH_WANT_SYSCALL_DEPRECATED
#include <unistd.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::bpf_dsl::AllOf;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {
namespace policy {

CrosAmdGpuProcessPolicy::CrosAmdGpuProcessPolicy() {}

CrosAmdGpuProcessPolicy::~CrosAmdGpuProcessPolicy() {}

ResultExpr CrosAmdGpuProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_sched_setaffinity:
    case __NR_sched_setscheduler:
    case __NR_sysinfo:
    case __NR_uname:
#if !defined(__aarch64__)
    case __NR_readlink:
    case __NR_stat:
#endif
      return Allow();
#if defined(__x86_64__)
    // Allow only AF_UNIX for |domain|.
    case __NR_socket:
    case __NR_socketpair: {
      const Arg<int> domain(0);
      return If(domain == AF_UNIX, Allow()).Else(Error(EPERM));
    }
#endif
    case __NR_kcmp: {
      const Arg<int> pid1(0);
      const Arg<int> pid2(1);
      const Arg<int> type(2);
      const int policy_pid = GetPolicyPid();
      // Only allowed when comparing file handles for the calling thread.
      return If(AllOf(pid1 == policy_pid, pid2 == policy_pid,
                      type == KCMP_FILE),
                Allow())
          .Else(Error(EPERM));
    }
    default:
      // Default to the generic GPU policy.
      return GpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace policy
}  // namespace sandbox
