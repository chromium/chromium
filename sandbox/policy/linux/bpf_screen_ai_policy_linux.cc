// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_screen_ai_policy_linux.h"

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
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
    case __NR_getitimer:
    case __NR_setitimer: {
      const Arg<int> which(0);
      return If(which == ITIMER_PROF, Allow()).Else(Error(EPERM));
    }
    case __NR_get_mempolicy: {
      const Arg<unsigned long> which(4);
      return If(which == 0, Allow()).Else(Error(EPERM));
    }
    case __NR_sched_getaffinity:
      return RestrictSchedTarget(GetPolicyPid(), system_call_number);

    default:
      return BPFBasePolicy::EvaluateSyscall(system_call_number);
  }
}

}  // namespace sandbox::policy
