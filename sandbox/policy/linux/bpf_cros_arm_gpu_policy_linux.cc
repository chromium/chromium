// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_cros_arm_gpu_policy_linux.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"
#include "sandbox/policy/linux/sandbox_seccomp_bpf_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {
namespace policy {

CrosArmGpuProcessPolicy::CrosArmGpuProcessPolicy(bool allow_shmat)
#if defined(__arm__) || defined(__aarch64__)
    : allow_shmat_(allow_shmat)
#endif
{
}

CrosArmGpuProcessPolicy::~CrosArmGpuProcessPolicy() {}

ResultExpr CrosArmGpuProcessPolicy::EvaluateSyscall(int sysno) const {
#if defined(__arm__) || defined(__aarch64__)
  if (allow_shmat_ && sysno == __NR_shmat)
    return Allow();
#endif  // defined(__arm__) || defined(__aarch64__)

  switch (sysno) {
#if defined(__arm__) || defined(__aarch64__)
    // ARM GPU sandbox is started earlier so we need to allow networking
    // in the sandbox.
    case __NR_connect:
    case __NR_getpeername:
    case __NR_getsockname:
    case __NR_sched_setaffinity:
    case __NR_sched_setscheduler:
    case __NR_sysinfo:
    case __NR_uname:
      return Allow();
    // Allow only AF_UNIX for |domain|.
    case __NR_socket:
    case __NR_socketpair: {
      const Arg<int> domain(0);
      return If(domain == AF_UNIX, Allow()).Else(Error(EPERM));
    }
#endif  // defined(__arm__) || defined(__aarch64__)
    default:
      // Default to the generic GPU policy.
      return GpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace policy
}  // namespace sandbox
