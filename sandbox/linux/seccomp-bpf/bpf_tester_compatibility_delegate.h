// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTER_COMPATIBILITY_DELEGATE_H_
#define SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTER_COMPATIBILITY_DELEGATE_H_

#include <memory>

#include "sandbox/linux/seccomp-bpf/sandbox_bpf_test_runner.h"

namespace sandbox {

// This templated class allows building a BPFTesterDelegate from a
// deprecated-style BPF policy (that is a SyscallEvaluator function pointer,
// instead of a SandboxBPFPolicy class), specified in |policy_function| and a
// function pointer to a test in |test_function|.
// This allows both the policy and the test function to take a pointer to an
// object of type "Aux" as a parameter. This is used to implement the BPF_TEST
// macro and should generally not be used directly.
template <class Policy, class Aux>
class BPFTesterCompatibilityDelegate : public BPFTesterDelegate {
 public:
  typedef void (*TestFunction)(Aux*);

  explicit BPFTesterCompatibilityDelegate(TestFunction test_function)
      : aux_(), test_function_(test_function) {}

  BPFTesterCompatibilityDelegate(const BPFTesterCompatibilityDelegate&) =
      delete;
  BPFTesterCompatibilityDelegate& operator=(
      const BPFTesterCompatibilityDelegate&) = delete;

  ~BPFTesterCompatibilityDelegate() override {}

  std::unique_ptr<bpf_dsl::Policy> GetSandboxBPFPolicy() override {
    // The current method is guaranteed to only run in the child process
    // running the test. In this process, the current object is guaranteed
    // to live forever. So it's ok to pass aux_pointer_for_policy_ to
    // the policy, which could in turn pass it to the kernel via Trap().
    return std::unique_ptr<bpf_dsl::Policy>(new Policy(&aux_));
  }

  void RunTestFunction() override {
    // Run the actual test.
    // The current object is guaranteed to live forever in the child process
    // where this will run.
    test_function_(&aux_);
  }

 private:
  Aux aux_;
  TestFunction test_function_;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTER_COMPATIBILITY_DELEGATE_H_
