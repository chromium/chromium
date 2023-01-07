// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
#define SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__

#include <memory>

#include "base/check.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf/bpf_tester_compatibility_delegate.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

// BPF_TEST_C() is a special version of SANDBOX_TEST(). It runs a test function
// in a sub-process, under a seccomp-bpf policy specified in
// |bpf_policy_class_name| without failing on configurations that are allowed
// to not support seccomp-bpf in their kernels.
// This is the preferred format for new BPF tests. |bpf_policy_class_name| is a
// class name  (which will be default-constructed) that implements the
// Policy interface.
// The test function's body can simply follow. Test functions should use
// the BPF_ASSERT macros defined below, not GTEST's macros. The use of
// CHECK* macros is supported but less robust.
#define BPF_TEST_C(test_case_name, test_name, bpf_policy_class_name)     \
  BPF_DEATH_TEST_C(                                                      \
      test_case_name, test_name, DEATH_SUCCESS(), bpf_policy_class_name)

// Identical to BPF_TEST_C but allows to specify the nature of death.
#define BPF_DEATH_TEST_C(                                            \
    test_case_name, test_name, death, bpf_policy_class_name)         \
  void BPF_TEST_C_##test_name();                                     \
  TEST(test_case_name, DISABLE_ON_TSAN(test_name)) {                 \
    sandbox::SandboxBPFTestRunner bpf_test_runner(                   \
        new sandbox::BPFTesterSimpleDelegate<bpf_policy_class_name>( \
            BPF_TEST_C_##test_name));                                \
    sandbox::UnitTests::RunTestInProcess(&bpf_test_runner, death);   \
  }                                                                  \
  void BPF_TEST_C_##test_name()

// This form of BPF_TEST is a little verbose and should be reserved for complex
// tests where a lot of control is required.
// |bpf_tester_delegate_class| must be a classname implementing the
// BPFTesterDelegate interface.
#define BPF_TEST_D(test_case_name, test_name, bpf_tester_delegate_class)     \
  BPF_DEATH_TEST_D(                                                          \
      test_case_name, test_name, DEATH_SUCCESS(), bpf_tester_delegate_class)

// Identical to BPF_TEST_D but allows to specify the nature of death.
#define BPF_DEATH_TEST_D(                                          \
    test_case_name, test_name, death, bpf_tester_delegate_class)   \
  TEST(test_case_name, DISABLE_ON_TSAN(test_name)) {               \
    sandbox::SandboxBPFTestRunner bpf_test_runner(                 \
        new bpf_tester_delegate_class());                          \
    sandbox::UnitTests::RunTestInProcess(&bpf_test_runner, death); \
  }

// Assertions are handled exactly the same as with a normal SANDBOX_TEST()
#define BPF_ASSERT SANDBOX_ASSERT
#define BPF_ASSERT_EQ(x, y) BPF_ASSERT((x) == (y))
#define BPF_ASSERT_NE(x, y) BPF_ASSERT((x) != (y))
#define BPF_ASSERT_LT(x, y) BPF_ASSERT((x) < (y))
#define BPF_ASSERT_GT(x, y) BPF_ASSERT((x) > (y))
#define BPF_ASSERT_LE(x, y) BPF_ASSERT((x) <= (y))
#define BPF_ASSERT_GE(x, y) BPF_ASSERT((x) >= (y))

// This form of BPF_TEST is now discouraged (but still allowed) in favor of
// BPF_TEST_D and BPF_TEST_C.
// The |policy| parameter should be a Policy subclass.
// BPF_TEST() takes a C++ data type as an fourth parameter. A variable
// of this type will be allocated and a pointer to it will be
// available within the test function as "BPF_AUX". The pointer will
// also be passed as an argument to the policy's constructor. Policies
// would typically use it as an argument to SandboxBPF::Trap(), if
// they want to communicate data between the BPF_TEST() and a Trap()
// function. The life-time of this object is the same as the life-time
// of the process running under the seccomp-bpf policy.
// |aux| must not be void.
#define BPF_TEST(test_case_name, test_name, policy, aux) \
  BPF_DEATH_TEST(test_case_name, test_name, DEATH_SUCCESS(), policy, aux)

// A BPF_DEATH_TEST is just the same as a BPF_TEST, but it assumes that the
// test will fail with a particular known error condition. Use the DEATH_XXX()
// macros from unit_tests.h to specify the expected error condition.
#define BPF_DEATH_TEST(test_case_name, test_name, death, policy, aux) \
  void BPF_TEST_##test_name(aux* BPF_AUX);                            \
  TEST(test_case_name, DISABLE_ON_TSAN(test_name)) {                  \
    sandbox::SandboxBPFTestRunner bpf_test_runner(                    \
        new sandbox::BPFTesterCompatibilityDelegate<policy, aux>(     \
            BPF_TEST_##test_name));                                   \
    sandbox::UnitTests::RunTestInProcess(&bpf_test_runner, death);    \
  }                                                                   \
  void BPF_TEST_##test_name(aux* BPF_AUX)

// This class takes a simple function pointer as a constructor parameter and a
// class name as a template parameter to implement the BPFTesterDelegate
// interface which can be used to build BPF unittests with
// the SandboxBPFTestRunner class.
template <class PolicyClass>
class BPFTesterSimpleDelegate : public BPFTesterDelegate {
 public:
  explicit BPFTesterSimpleDelegate(void (*test_function)(void))
      : test_function_(test_function) {}

  BPFTesterSimpleDelegate(const BPFTesterSimpleDelegate&) = delete;
  BPFTesterSimpleDelegate& operator=(const BPFTesterSimpleDelegate&) = delete;

  ~BPFTesterSimpleDelegate() override {}

  std::unique_ptr<bpf_dsl::Policy> GetSandboxBPFPolicy() override {
    return std::unique_ptr<bpf_dsl::Policy>(new PolicyClass());
  }
  void RunTestFunction() override {
    DCHECK(test_function_);
    test_function_();
  }

 private:
  void (*test_function_)(void);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_BPF_TESTS_H__
