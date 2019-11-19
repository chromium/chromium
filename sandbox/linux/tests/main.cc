// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {
namespace {

// Check for leaks in our tests.
void RunPostTestsChecks(const base::FilePath& orig_cwd) {
  if (TestUtils::CurrentProcessHasChildren()) {
    LOG(FATAL) << "One of the tests created a child that was not waited for. "
               << "Please, clean up after your tests!";
  }

  base::FilePath cwd;
  CHECK(GetCurrentDirectory(&cwd));
  if (orig_cwd != cwd) {
    LOG(FATAL) << "One of the tests changed the current working directory. "
               << "Please, clean up after your tests!";
  }
}

}  // namespace
}  // namespace sandbox

#if !defined(SANDBOX_USES_BASE_TEST_SUITE)
void UnitTestAssertHandler(const char* file,
                           int line,
                           const base::StringPiece message,
                           const base::StringPiece stack_trace) {
  _exit(1);
}
#endif

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  std::string client_func;
#if defined(SANDBOX_USES_BASE_TEST_SUITE)
  client_func = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kTestChildProcess);
#endif
  if (!client_func.empty()) {
    base::AtExitManager exit_manager;
    return multi_process_function_list::InvokeChildProcessTest(client_func);
  }

  base::FilePath orig_cwd;
  CHECK(GetCurrentDirectory(&orig_cwd));

#if !defined(SANDBOX_USES_BASE_TEST_SUITE)
  // The use of Callbacks requires an AtExitManager.
  base::AtExitManager exit_manager;
  testing::InitGoogleTest(&argc, argv);
  // Death tests rely on LOG(FATAL) triggering an exit (the default behavior is
  // SIGABRT).  The normal test launcher does this at initialization, but since
  // we still do not use this on Android, we must install the handler ourselves.
  logging::ScopedLogAssertHandler scoped_assert_handler(
      base::BindRepeating(UnitTestAssertHandler));
#endif
  // Always go through re-execution for death tests.
  // This makes gtest only marginally slower for us and has the
  // additional side effect of getting rid of gtest warnings about fork()
  // safety.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
#if !defined(SANDBOX_USES_BASE_TEST_SUITE)
  int tests_result = RUN_ALL_TESTS();
#else
  int tests_result = base::RunUnitTestsUsingBaseTestSuite(argc, argv);
#endif

  sandbox::RunPostTestsChecks(orig_cwd);
  return tests_result;
}
