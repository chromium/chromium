// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf_test_runner.h"

#include <fcntl.h>

#include <memory>

#include "base/check.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/system_headers/linux_filter.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

SandboxBPFTestRunner::SandboxBPFTestRunner(
    BPFTesterDelegate* bpf_tester_delegate)
    : bpf_tester_delegate_(bpf_tester_delegate) {
}

SandboxBPFTestRunner::~SandboxBPFTestRunner() {
}

void SandboxBPFTestRunner::Run() {
  DCHECK(bpf_tester_delegate_);
  sandbox::Die::EnableSimpleExit();

  std::unique_ptr<bpf_dsl::Policy> policy =
      bpf_tester_delegate_->GetSandboxBPFPolicy();

  if (sandbox::SandboxBPF::SupportsSeccompSandbox(
          SandboxBPF::SeccompLevel::SINGLE_THREADED)) {
    // Initialize and then start the sandbox with our custom policy
    sandbox::SandboxBPF sandbox(std::move(policy));
    SANDBOX_ASSERT(sandbox.StartSandbox(
        sandbox::SandboxBPF::SeccompLevel::SINGLE_THREADED));

    // Run the actual test.
    bpf_tester_delegate_->RunTestFunction();
  } else {
    printf("This BPF test is not fully running in this configuration!\n");
    // Android is the only configuration where we accept not having kernel
    // BPF support.
    if (!IsAndroid()) {
      const bool seccomp_bpf_is_supported = false;
      SANDBOX_ASSERT(seccomp_bpf_is_supported);
    }
    // Call the compiler and verify the policy. That's the least we can do,
    // if we don't have kernel support.
    sandbox::SandboxBPF sandbox(std::move(policy));
    sandbox.AssembleFilter();
    sandbox::UnitTests::IgnoreThisTest();
  }
}

bool SandboxBPFTestRunner::ShouldCheckForLeaks() const {
  // LSAN requires being able to use ptrace() and other system calls that could
  // be denied.
  return false;
}

}  // namespace sandbox
