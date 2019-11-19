// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_
#define CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "build/build_config.h"

namespace base {
class CommandLine;
class FilePath;
struct TestResult;
}

namespace content {
class ContentMainDelegate;
struct ContentMainParams;

class TestLauncherDelegate {
 public:
  virtual int RunTestSuite(int argc, char** argv) = 0;
  virtual bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) = 0;
#if !defined(OS_ANDROID)
  // Android browser tests set the ContentMainDelegate itself for the test
  // harness to use, and do not go through ContentMain() in TestLauncher.
  virtual ContentMainDelegate* CreateContentMainDelegate() = 0;
#endif

  // Called prior to running each test.
  //
  // NOTE: this is not called if --single-process-tests is supplied.
  virtual void PreRunTest() {}

  // Called after running each test. Can modify test result.
  //
  // NOTE: Just like PreRunTest, this is not called when --single-process-tests
  // is supplied.
  virtual void PostRunTest(base::TestResult* result) {}

  // Allows a TestLauncherDelegate to do work before the launcher shards test
  // jobs.
  virtual void PreSharding() {}

  // Invoked when a child process times out immediately before it is terminated.
  // |command_line| is the command line of the child process.
  virtual void OnTestTimedOut(const base::CommandLine& command_line) {}

  // Called prior to returning from LaunchTests(). Gives the delegate a chance
  // to do cleanup before state created by TestLauncher has been destroyed (such
  // as the AtExitManager).
  virtual void OnDoneRunningTests() {}

 protected:
  virtual ~TestLauncherDelegate() = default;
};

// Launches tests using |launcher_delegate|. |parallel_jobs| is the number
// of test jobs to be run in parallel.
int LaunchTests(TestLauncherDelegate* launcher_delegate,
                size_t parallel_jobs,
                int argc,
                char** argv) WARN_UNUSED_RESULT;

TestLauncherDelegate* GetCurrentTestLauncherDelegate();

#if !defined(OS_ANDROID)
// ContentMain is not run on Android in the test process, and is run via
// java for child processes. So ContentMainParams does not exist there.
ContentMainParams* GetContentMainParams();
#endif

// Returns true if the currently running test has a prefix that indicates it
// should run before a test of the same name without the prefix.
bool IsPreTest();

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_LAUNCHER_H_
