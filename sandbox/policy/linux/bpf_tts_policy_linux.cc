// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_tts_policy_linux.h"

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

TtsProcessPolicy::TtsProcessPolicy() {}

TtsProcessPolicy::~TtsProcessPolicy() {}

ResultExpr TtsProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_getcpu:
    case __NR_sysinfo:
      return Allow();
    default:
      break;
  }

  auto* sandbox_linux = SandboxLinux::GetInstance();
  if (sandbox_linux->ShouldBrokerHandleSyscall(sysno))
    return sandbox_linux->HandleViaBroker(sysno);

  return BPFBasePolicy::EvaluateSyscall(sysno);
}

}  // namespace policy
}  // namespace sandbox
