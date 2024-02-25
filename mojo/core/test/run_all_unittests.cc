// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "mojo/core/test/mojo_test_suite_base.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
#if !BUILDFLAG(IS_ANDROID)
  // Silence death test thread warnings on Linux. We can afford to run our death
  // tests a little more slowly (< 10 ms per death test on a Z620).
  // On android, we need to run in the default mode, as the threadsafe mode
  // relies on execve which is not available.
  GTEST_FLAG_SET(death_test_style, "threadsafe");
#endif
#if BUILDFLAG(IS_ANDROID)
  // On android, the test framework has a signal handler that will print a
  // [ CRASH ] line when the application crashes. This breaks death test has the
  // test runner will consider the death of the child process a test failure.
  // Removing the signal handler solves this issue.
  signal(SIGABRT, SIG_DFL);
#endif

  mojo::core::test::MojoTestSuiteBase test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
