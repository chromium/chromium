// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_network_policy_linux.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/bpf_base_policy_linux.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "sandbox/policy/linux/sandbox_seccomp_bpf_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerProcess;

namespace sandbox {
namespace policy {

NetworkProcessPolicy::NetworkProcessPolicy() {}

NetworkProcessPolicy::~NetworkProcessPolicy() {}

ResultExpr NetworkProcessPolicy::EvaluateSyscall(int sysno) const {
  auto* sandbox_linux = SandboxLinux::GetInstance();
  if (sandbox_linux->ShouldBrokerHandleSyscall(sysno))
    return sandbox_linux->HandleViaBroker(sysno);

  // TODO(mpdenton): FIX this.
  return Allow();
}

}  // namespace policy
}  // namespace sandbox
