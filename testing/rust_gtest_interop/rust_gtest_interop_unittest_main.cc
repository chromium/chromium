// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "build/blink_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <iostream>

// Update this when adding a new test to rust_test_interop_unittest.rs.
int kNumTests = 10;

bool IsSubprocess() {
#if BUILDFLAG(USE_BLINK)
  // The test launching process spawns a subprocess to run tests, and it
  // includes this flag.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      base::kGTestFlagfileFlag);
#else
  // This function should only be called if the standard test launcher is
  // being used.
  NOTREACHED();
#endif
}

int VerifyTestsRan() {
  // Double-check that we actually ran all the tests. If this fails we'll
  // see all the tests marked as "fail on exit" since the whole process is
  // considered a failure.
  auto succeed = testing::UnitTest::GetInstance()->successful_test_count();
  int expected_success = kNumTests;
  if (succeed != expected_success) {
    std::cerr << "***ERROR***: Expected " << expected_success
              << " tests to succeed, but we saw: " << succeed << '\n';
    return 1;
  } else {
    std::cerr << "***OK***: Ran " << succeed << " tests, yay!\n";
    return 0;
  }
}

int main(int argc, char** argv) {
  // Run tests in a single process so we can count the tests.
  std::string single_process = base::StringPrintf("--test-launcher-jobs=1");
  // We verify that the test suite and test name written in the #[gtest] macro
  // is being propagated to Gtest by using a test filter that matches on the
  // test suites/names.
  std::string filter = "--gtest_filter=Test.*:ExactSuite.ExactTest";

  auto my_argv = std::vector<char*>();
  for (int i = 0; i < argc; ++i) {
    my_argv.push_back(argv[i]);
  }
  my_argv.push_back(single_process.data());
  my_argv.push_back(filter.data());
  my_argv.push_back(nullptr);  // GTest reads past argc until null.

  struct InteropTestSuite : public base::TestSuite {
    InteropTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

    int Run() {
      int result = base::TestSuite::Run();
#if !BUILDFLAG(USE_BLINK)
      // If the tests ran in the main process (on iOS), we'll verify here.
      if (!result) {
        return VerifyTestsRan();
      }
#endif
      return result;
    }
  };

  InteropTestSuite test_suite(my_argv.size() - 1u, my_argv.data());
  // Note: With the iOS test launcher, this does not return, so we do the check
  // for which tests ran in the TestSuite.
  int result = base::LaunchUnitTests(
      my_argv.size() - 1u, my_argv.data(),
      base::BindOnce(&InteropTestSuite::Run, base::Unretained(&test_suite)));
  // If the tests ran in a child process, we'll verify here.
  if (!result && IsSubprocess()) {
    return VerifyTestsRan();
  }
  return result;
}
