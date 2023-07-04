// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <iostream>

// Update this when adding a new test to rust_test_interop_unittest.rs.
int kNumTests = 10;

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
  std::string filter = "--gtest_filter=Test.*:ExactSuite.ExactTest";

  auto my_argv = std::vector<char*>();
  for (int i = 0; i < argc; ++i) {
    my_argv.push_back(argv[i]);
  }
  my_argv.push_back(single_process.data());
  my_argv.push_back(filter.data());
  my_argv.push_back(nullptr);  // GTest reads past argc until null.

  base::TestSuite test_suite(my_argv.size() - 1u, my_argv.data());

  int result = base::LaunchUnitTests(
      my_argv.size() - 1u, my_argv.data(),
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
    } else {
      std::cerr << "***OK***: Ran " << succeed << " tests, yay!\n";
    }
  }

  return result;
}
