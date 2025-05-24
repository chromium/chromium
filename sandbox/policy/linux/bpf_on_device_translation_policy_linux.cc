// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_on_device_translation_policy_linux.h"

#include <sys/mman.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox::policy {

namespace {
static constexpr int kMEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_RSEQ = (1 << 8);
}

ResultExpr OnDeviceTranslationProcessPolicy::EvaluateSyscall(
    int system_call_number) const {
  switch (system_call_number) {
    case __NR_membarrier: {
      // `membarrier` is used at http://shortn/_d034oISVml (Google-internal).
      const Arg<int> cmd(0);
      return If(cmd == kMEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_RSEQ, Allow())
          .Else(sandbox::CrashSIGSYS());
    }
    default:
      auto* sandbox_linux = SandboxLinux::GetInstance();
      if (sandbox_linux->ShouldBrokerHandleSyscall(system_call_number)) {
        return sandbox_linux->HandleViaBroker(system_call_number);
      }

      // Default on the content baseline policy.
      return BPFBasePolicy::EvaluateSyscall(system_call_number);
  }
}

}  // namespace sandbox::policy
