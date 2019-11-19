// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int RunHelper(base::TestSuite* test_suite) {
  base::SingleThreadTaskExecutor task_executor;
  return test_suite->Run();
}

}  // namespace

// Defined in angle_deqp_gtest.cpp. Declared here so we don't need to make a
// header that we import in Chromium.
namespace angle {
void InitTestHarness(int* argc, char** argv);
}

int main(int argc, char** argv) {
  // base::CommandLine::Init must be called before angle::InitTestHarness,
  // because angle::InitTestHarness deletes ANGLE-specific arguments from argv.
  // But, on Linux, tests are run in ChildGTestProcess, which inherits its
  // command line from the one initialized in base::CommandLine::Init.
  // In this order, ChildGTestProcess inherits all the ANGLE-specific
  // arguments that it requires.
  base::CommandLine::Init(argc, argv);
  angle::InitTestHarness(&argc, argv);
  base::TestSuite test_suite(argc, argv);

  // The process and thread priorities are modified by
  // StabilizeCPUForBenchmarking()/SetLowPriorityProcess().
  test_suite.DisableCheckForThreadAndProcessPriority();

  int rt = base::LaunchUnitTestsSerially(
      argc, argv, base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
  return rt;
}
