// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_TESTS_UNIT_TESTS_H_
#define SANDBOX_LINUX_TESTS_UNIT_TESTS_H_

#include <sys/syscall.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "sandbox/linux/tests/sandbox_test_runner_function_pointer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Different platforms use different symbols for the six-argument version
// of the mmap() system call. Test for the correct symbol at compile time.
#ifdef __NR_mmap2
const int kMMapNr = __NR_mmap2;
#else
const int kMMapNr = __NR_mmap;
#endif

// Has this been compiled to run on Android?
bool IsAndroid();

bool IsArchitectureArm();

#if defined(ADDRESS_SANITIZER)
#define DISABLE_ON_ASAN(test_name) DISABLED_##test_name
#else
#define DISABLE_ON_ASAN(test_name) test_name
#endif  // defined(ADDRESS_SANITIZER)

#if defined(LEAK_SANITIZER)
#define DISABLE_ON_LSAN(test_name) DISABLED_##test_name
#else
#define DISABLE_ON_LSAN(test_name) test_name
#endif

#if defined(THREAD_SANITIZER)
#define DISABLE_ON_TSAN(test_name) DISABLED_##test_name
#else
#define DISABLE_ON_TSAN(test_name) test_name
#endif  // defined(THREAD_SANITIZER)

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER) || defined(LEAK_SANITIZER) ||    \
    defined(UNDEFINED_SANITIZER) || defined(SANITIZER_COVERAGE)
#define DISABLE_ON_SANITIZERS(test_name) DISABLED_##test_name
#else
#define DISABLE_ON_SANITIZERS(test_name) test_name
#endif

#if defined(OS_ANDROID)
#define DISABLE_ON_ANDROID(test_name) DISABLED_##test_name
#else
#define DISABLE_ON_ANDROID(test_name) test_name
#endif

// While it is perfectly OK for a complex test to provide its own DeathCheck
// function. Most death tests have very simple requirements. These tests should
// use one of the predefined DEATH_XXX macros as an argument to
// SANDBOX_DEATH_TEST(). You can check for a (sub-)string in the output of the
// test, for a particular exit code, or for a particular death signal.
// NOTE: If you do decide to write your own DeathCheck, make sure to use
//       gtests's ASSERT_XXX() macros instead of SANDBOX_ASSERT(). See
//       unit_tests.cc for examples.
#define DEATH_SUCCESS() sandbox::UnitTests::DeathSuccess, NULL
#define DEATH_SUCCESS_ALLOW_NOISE() \
  sandbox::UnitTests::DeathSuccessAllowNoise, NULL
#define DEATH_MESSAGE(msg)          \
  sandbox::UnitTests::DeathMessage, \
      static_cast<const void*>(static_cast<const char*>(msg))
#define DEATH_SEGV_MESSAGE(msg)         \
  sandbox::UnitTests::DeathSEGVMessage, \
      static_cast<const void*>(static_cast<const char*>(msg))
#define DEATH_EXIT_CODE(rc)          \
  sandbox::UnitTests::DeathExitCode, \
      reinterpret_cast<void*>(static_cast<intptr_t>(rc))
#define DEATH_BY_SIGNAL(s)           \
  sandbox::UnitTests::DeathBySignal, \
      reinterpret_cast<void*>(static_cast<intptr_t>(s))

// A SANDBOX_DEATH_TEST is just like a SANDBOX_TEST (see below), but it assumes
// that the test actually dies. The death test only passes if the death occurs
// in the expected fashion, as specified by "death" and "death_aux". These two
// parameters are typically set to one of the DEATH_XXX() macros.
#define SANDBOX_DEATH_TEST(test_case_name, test_name, death)                \
  void TEST_##test_name(void);                                              \
  TEST(test_case_name, test_name) {                                         \
    SandboxTestRunnerFunctionPointer sandbox_test_runner(TEST_##test_name); \
    sandbox::UnitTests::RunTestInProcess(&sandbox_test_runner, death);      \
  }                                                                         \
  void TEST_##test_name(void)

// Define a new test case that runs inside of a GTest death test. This is
// necessary, as most of our tests by definition make global and irreversible
// changes to the system (i.e. they install a sandbox). GTest provides death
// tests as a tool to isolate global changes from the rest of the tests.
#define SANDBOX_TEST(test_case_name, test_name) \
  SANDBOX_DEATH_TEST(test_case_name, test_name, DEATH_SUCCESS())

// SANDBOX_TEST_ALLOW_NOISE is just like SANDBOX_TEST, except it does not
// consider log error messages printed by the test to be test failures.
#define SANDBOX_TEST_ALLOW_NOISE(test_case_name, test_name) \
  SANDBOX_DEATH_TEST(test_case_name, test_name, DEATH_SUCCESS_ALLOW_NOISE())

// Simple assertion macro that is compatible with running inside of a death
// test. We unfortunately cannot use any of the GTest macros.
#define SANDBOX_STR(x) #x
#define SANDBOX_ASSERT(expr)                                             \
  ((expr) ? static_cast<void>(0) : sandbox::UnitTests::AssertionFailure( \
                                       SANDBOX_STR(expr), __FILE__, __LINE__))

#define SANDBOX_ASSERT_EQ(x, y) SANDBOX_ASSERT((x) == (y))
#define SANDBOX_ASSERT_NE(x, y) SANDBOX_ASSERT((x) != (y))
#define SANDBOX_ASSERT_LT(x, y) SANDBOX_ASSERT((x) < (y))
#define SANDBOX_ASSERT_GT(x, y) SANDBOX_ASSERT((x) > (y))
#define SANDBOX_ASSERT_LE(x, y) SANDBOX_ASSERT((x) <= (y))
#define SANDBOX_ASSERT_GE(x, y) SANDBOX_ASSERT((x) >= (y))

// This class allows to run unittests in their own process. The main method is
// RunTestInProcess().
class UnitTests {
 public:
  typedef void (*DeathCheck)(int status,
                             const std::string& msg,
                             const void* aux);

  // Runs a test inside a short-lived process. Do not call this function
  // directly. It is automatically invoked by SANDBOX_TEST(). Most sandboxing
  // functions make global irreversible changes to the execution environment
  // and must therefore execute in their own isolated process.
  // |test_runner| must implement the SandboxTestRunner interface and will run
  // in a subprocess.
  // Note: since the child process (created with fork()) will never return from
  // RunTestInProcess(), |test_runner| is guaranteed to exist for the lifetime
  // of the child process.
  static void RunTestInProcess(SandboxTestRunner* test_runner,
                               DeathCheck death,
                               const void* death_aux);

  // Report a useful error message and terminate the current SANDBOX_TEST().
  // Calling this function from outside a SANDBOX_TEST() is unlikely to do
  // anything useful.
  static void AssertionFailure(const char* expr, const char* file, int line);

  // Sometimes we determine at run-time that a test should be disabled.
  // Call this method if we want to return from a test and completely
  // ignore its results.
  // You should not call this method, if the test already ran any test-relevant
  // code. Most notably, you should not call it, you already wrote any messages
  // to stderr.
  static void IgnoreThisTest();

  // A DeathCheck method that verifies that the test completed successfully.
  // This is the default test mode for SANDBOX_TEST(). The "aux" parameter
  // of this DeathCheck is unused (and thus unnamed)
  static void DeathSuccess(int status, const std::string& msg, const void*);

  // A DeathCheck method that verifies that the test completed successfully
  // allowing for log error messages.
  static void DeathSuccessAllowNoise(int status,
                                     const std::string& msg,
                                     const void*);

  // A DeathCheck method that verifies that the test completed with error
  // code "1" and printed a message containing a particular substring. The
  // "aux" pointer should point to a C-string containing the expected error
  // message. This method is useful for checking assertion failures such as
  // in SANDBOX_ASSERT() and/or SANDBOX_DIE().
  static void DeathMessage(int status, const std::string& msg, const void* aux);

  // Like DeathMessage() but the process must be terminated with a segmentation
  // fault.
  // Implementation detail: On Linux (but not on Android), this does check for
  // the return value of our default signal handler rather than for the actual
  // reception of a SIGSEGV.
  // TODO(jln): make this more robust.
  static void DeathSEGVMessage(int status,
                               const std::string& msg,
                               const void* aux);

  // A DeathCheck method that verifies that the test completed with a
  // particular exit code. If the test output any messages to stderr, they are
  // silently ignored. The expected exit code should be passed in by
  // casting the its "int" value to a "void *", which is then used for "aux".
  static void DeathExitCode(int status,
                            const std::string& msg,
                            const void* aux);

  // A DeathCheck method that verifies that the test was terminated by a
  // particular signal. If the test output any messages to stderr, they are
  // silently ignore. The expected signal number should be passed in by
  // casting the its "int" value to a "void *", which is then used for "aux".
  static void DeathBySignal(int status,
                            const std::string& msg,
                            const void* aux);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UnitTests);
};

}  // namespace

#endif  // SANDBOX_LINUX_TESTS_UNIT_TESTS_H_
