// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <iostream>

// Update this when adding a new test to rust_test_interop_unittest.rs.
int kNumTests = 9;

bool is_subprocess() {
  // The test launching process spawns a subprocess to run tests, and it
  // includes this flag.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      base::kGTestFlagfileFlag);
}

int main(int argc, char** argv) {
  // Run tests in a single process so we can count the tests.
  std::string single_process = base::StringPrintf("--test-launcher-jobs=1");
  // We verify that the test suite and test name written in the #[gtest] macro
  // is being propagated to Gtest by using a test filter that matches on the
  // test suites/names.
  std::string filter =
      base::StringPrintf("--gtest_filter=Test.*:ExactSuite.ExactTest");

  int my_argc = argc + 2;
  char** my_argv = new char*[argc];
  for (int i = 0; i < argc; ++i)
    my_argv = argv;
  my_argv[argc] = single_process.data();
  my_argv[argc + 1] = filter.data();

  base::TestSuite test_suite(my_argc, my_argv);

  int result = base::LaunchUnitTests(
      my_argc, my_argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));

  if (is_subprocess()) {
    // Double-check that we actually ran all the tests. If this fails we'll see
    // all the tests marked as "fail on exit" since the whole process is
    // considered a failure.
    auto succeed = testing::UnitTest::GetInstance()->successful_test_count();
    int expected_success = kNumTests;
    if (succeed != expected_success) {
      std::cerr << "***ERROR***: Expected " << expected_success
                << " tests to succeed, but we saw: " << succeed << '\n';
      return 1;
    }
  }

  return result;
}
