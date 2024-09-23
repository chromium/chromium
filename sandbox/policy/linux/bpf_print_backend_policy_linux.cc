// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_print_backend_policy_linux.h"

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/policy/linux/sandbox_linux.h"

namespace sandbox::policy {

PrintBackendProcessPolicy::PrintBackendProcessPolicy() = default;
PrintBackendProcessPolicy::~PrintBackendProcessPolicy() = default;

bpf_dsl::ResultExpr PrintBackendProcessPolicy::EvaluateSyscall(
    int sysno) const {
  auto* sandbox_linux = SandboxLinux::GetInstance();
  if (sandbox_linux->ShouldBrokerHandleSyscall(sysno)) {
    return sandbox_linux->HandleViaBroker(sysno);
  }

  // TODO(crbug.com/40896074): write a better syscall filter.
  return bpf_dsl::Allow();
}

}  // namespace sandbox::policy
