// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int RunHelper(base::TestSuite* test_suite) {
  base::SingleThreadTaskExecutor task_executor;
  return test_suite->Run();
}

}  // namespace

// Definition located in third_party/dawn/src/tests/DawnTest.h
// Forward declared here to avoid pulling in the Dawn headers.
void InitDawnEnd2EndTestEnvironment(int argc, char** argv);

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  testing::InitGoogleMock(&argc, argv);
  base::TestSuite test_suite(argc, argv);
  InitDawnEnd2EndTestEnvironment(argc, argv);
  int rt = base::LaunchUnitTestsWithOptions(
      argc, argv,
      1,     // Run tests serially.
      0,     // Disable batching.
      true,  // Use job objects.
      base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
  return rt;
}
