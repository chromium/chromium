// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_libassistant_policy_linux.h"

#include <sys/socket.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerProcess;

namespace sandbox {
namespace policy {

LibassistantProcessPolicy::LibassistantProcessPolicy() = default;
LibassistantProcessPolicy::~LibassistantProcessPolicy() = default;

ResultExpr LibassistantProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if defined(__NR_getcpu)
    // Needed by arm devices.
    case __NR_getcpu:
      return Allow();
#endif
#if defined(__NR_sched_setscheduler)
    case __NR_sched_setscheduler:
      return Allow();
#endif
    default:
      auto* sandbox_linux = SandboxLinux::GetInstance();
      if (sandbox_linux->ShouldBrokerHandleSyscall(sysno))
        return sandbox_linux->HandleViaBroker();

      return BPFBasePolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace policy
}  // namespace sandbox
