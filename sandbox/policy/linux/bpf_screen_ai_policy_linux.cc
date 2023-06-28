// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_screen_ai_policy_linux.h"

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/system_headers/linux_futex.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox::policy {

ScreenAIProcessPolicy::ScreenAIProcessPolicy() = default;
ScreenAIProcessPolicy::~ScreenAIProcessPolicy() = default;

ResultExpr ScreenAIProcessPolicy::EvaluateSyscall(
    int system_call_number) const {
  auto* sandbox_linux = SandboxLinux::GetInstance();
  if (sandbox_linux->ShouldBrokerHandleSyscall(system_call_number))
    return sandbox_linux->HandleViaBroker(system_call_number);

  switch (system_call_number) {
    case __NR_futex:
#if defined(__NR_futex_time64)
    case __NR_futex_time64:
#endif
    {
      const Arg<int> op(1);
      return Switch(op & FUTEX_CMD_MASK)
          .Cases(
              {FUTEX_CMP_REQUEUE, FUTEX_LOCK_PI, FUTEX_UNLOCK_PI, FUTEX_WAIT,
               FUTEX_WAIT_BITSET, FUTEX_WAKE},
              Allow())
          // Sending ENOSYS tells the Futex backend to use another approach if
          // this fails.
          .Default(Error(ENOSYS));
    }

    case __NR_getcpu:
      return Allow();

    case __NR_get_mempolicy: {
      const Arg<unsigned long> which(4);
      return If(which == 0, Allow()).Else(Error(EPERM));
    }

    case __NR_prlimit64:
      return RestrictPrlimitToGetrlimit(GetPolicyPid());

    case __NR_sysinfo:
      return Allow();

    default:
      if (SyscallSets::IsGoogle3Threading(system_call_number)) {
        return RestrictGoogle3Threading(system_call_number);
      }

      return BPFBasePolicy::EvaluateSyscall(system_call_number);
  }
}

}  // namespace sandbox::policy
