// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/bpf_tests.h"

#include <errno.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "base/logging.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {

namespace {

class FourtyTwo {
 public:
  static const int kMagicValue = 42;
  FourtyTwo() : value_(kMagicValue) {}

  FourtyTwo(const FourtyTwo&) = delete;
  FourtyTwo& operator=(const FourtyTwo&) = delete;

  int value() { return value_; }

 private:
  int value_;
};

class EmptyClassTakingPolicy : public bpf_dsl::Policy {
 public:
  explicit EmptyClassTakingPolicy(FourtyTwo* fourty_two) {
    BPF_ASSERT(fourty_two);
    BPF_ASSERT(FourtyTwo::kMagicValue == fourty_two->value());
  }
  ~EmptyClassTakingPolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));
    return Allow();
  }
};

BPF_TEST(BPFTest,
         BPFAUXPointsToClass,
         EmptyClassTakingPolicy,
         FourtyTwo /* *BPF_AUX */) {
  // BPF_AUX should point to an instance of FourtyTwo.
  BPF_ASSERT(BPF_AUX);
  BPF_ASSERT(FourtyTwo::kMagicValue == BPF_AUX->value());
}

void DummyTestFunction(FourtyTwo *fourty_two) {
}

TEST(BPFTest, BPFTesterCompatibilityDelegateLeakTest) {
  // Don't do anything, simply gives dynamic tools an opportunity to detect
  // leaks.
  {
    BPFTesterCompatibilityDelegate<EmptyClassTakingPolicy, FourtyTwo>
        simple_delegate(DummyTestFunction);
  }
  {
    // Test polymorphism.
    std::unique_ptr<BPFTesterDelegate> simple_delegate(
        new BPFTesterCompatibilityDelegate<EmptyClassTakingPolicy, FourtyTwo>(
            DummyTestFunction));
  }
}

class EnosysPtracePolicy : public bpf_dsl::Policy {
 public:
  EnosysPtracePolicy() { my_pid_ = sys_getpid(); }

  EnosysPtracePolicy(const EnosysPtracePolicy&) = delete;
  EnosysPtracePolicy& operator=(const EnosysPtracePolicy&) = delete;

  ~EnosysPtracePolicy() override {
    // Policies should be able to bind with the process on which they are
    // created. They should never be created in a parent process.
    BPF_ASSERT_EQ(my_pid_, sys_getpid());
  }

  ResultExpr EvaluateSyscall(int system_call_number) const override {
    CHECK(SandboxBPF::IsValidSyscallNumber(system_call_number));
    if (system_call_number == __NR_ptrace) {
      // The EvaluateSyscall function should run in the process that created
      // the current object.
      BPF_ASSERT_EQ(my_pid_, sys_getpid());
      return Error(ENOSYS);
    } else {
      return Allow();
    }
  }

 private:
  pid_t my_pid_;
};

class BasicBPFTesterDelegate : public BPFTesterDelegate {
 public:
  BasicBPFTesterDelegate() {}

  BasicBPFTesterDelegate(const BasicBPFTesterDelegate&) = delete;
  BasicBPFTesterDelegate& operator=(const BasicBPFTesterDelegate&) = delete;

  ~BasicBPFTesterDelegate() override {}

  std::unique_ptr<bpf_dsl::Policy> GetSandboxBPFPolicy() override {
    return std::unique_ptr<bpf_dsl::Policy>(new EnosysPtracePolicy());
  }
  void RunTestFunction() override {
    errno = 0;
    int ret = ptrace(PTRACE_TRACEME, -1, NULL, NULL);
    BPF_ASSERT(-1 == ret);
    BPF_ASSERT(ENOSYS == errno);
  }
};

// This is the most powerful and complex way to create a BPF test, but it
// requires a full class definition (BasicBPFTesterDelegate).
BPF_TEST_D(BPFTest, BPFTestWithDelegateClass, BasicBPFTesterDelegate)

// This is the simplest form of BPF tests.
BPF_TEST_C(BPFTest, BPFTestWithInlineTest, EnosysPtracePolicy) {
  errno = 0;
  int ret = ptrace(PTRACE_TRACEME, -1, NULL, NULL);
  BPF_ASSERT(-1 == ret);
  BPF_ASSERT(ENOSYS == errno);
}

const char kHelloMessage[] = "Hello";

BPF_DEATH_TEST_C(BPFTest,
                 BPFDeathTestWithInlineTest,
                 DEATH_MESSAGE(kHelloMessage),
                 EnosysPtracePolicy) {
  LOG(ERROR) << kHelloMessage;
  _exit(1);
}

}  // namespace

}  // namespace sandbox
