// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_TEST_RUNNER_H_
#define SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_TEST_RUNNER_H_

#include <memory>

#include "sandbox/linux/tests/sandbox_test_runner.h"

namespace sandbox {
namespace bpf_dsl {
class Policy;
}

// To create a SandboxBPFTestRunner object, one needs to implement this
// interface and pass an instance to the SandboxBPFTestRunner constructor.
// In the child process running the test, the BPFTesterDelegate object is
// guaranteed to not be destroyed until the child process terminates.
class BPFTesterDelegate {
 public:
  BPFTesterDelegate() {}

  BPFTesterDelegate(const BPFTesterDelegate&) = delete;
  BPFTesterDelegate& operator=(const BPFTesterDelegate&) = delete;

  virtual ~BPFTesterDelegate() {}

  // This will instanciate a policy suitable for the test we want to run. It is
  // guaranteed to only be called from the child process that will run the
  // test.
  virtual std::unique_ptr<bpf_dsl::Policy> GetSandboxBPFPolicy() = 0;
  // This will be called from a child process with the BPF sandbox turned on.
  virtual void RunTestFunction() = 0;
};

// This class implements the SandboxTestRunner interface and Run() will
// initialize a seccomp-bpf sandbox (specified by |bpf_tester_delegate|) and
// run a test function (via |bpf_tester_delegate|) if the current kernel
// configuration allows it. If it can not run the test under seccomp-bpf,
// Run() will still compile the policy which should allow to get some coverage
// under tools that behave like Valgrind.
class SandboxBPFTestRunner : public SandboxTestRunner {
 public:
  // This constructor takes ownership of the |bpf_tester_delegate| object.
  // (It doesn't take a std::unique_ptr since they make polymorphism verbose).
  explicit SandboxBPFTestRunner(BPFTesterDelegate* bpf_tester_delegate);

  SandboxBPFTestRunner(const SandboxBPFTestRunner&) = delete;
  SandboxBPFTestRunner& operator=(const SandboxBPFTestRunner&) = delete;

  ~SandboxBPFTestRunner() override;

  void Run() override;

  bool ShouldCheckForLeaks() const override;

 private:
  std::unique_ptr<BPFTesterDelegate> bpf_tester_delegate_;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_SANDBOX_BPF_TEST_RUNNER_H_
