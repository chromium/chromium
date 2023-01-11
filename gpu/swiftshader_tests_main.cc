// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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

class SwiftShaderTestEnvironment : public testing::Environment {
 public:
  void SetUp() override {}
  void TearDown() override {}
};

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  testing::InitGoogleMock(&argc, argv);
  testing::AddGlobalTestEnvironment(new SwiftShaderTestEnvironment());
  base::TestSuite test_suite(argc, argv);
  int rt = base::LaunchUnitTestsWithOptions(
      argc, argv,
      1,                  // Run tests serially.
      0,                  // Disable batching.
      true,               // Use job objects.
      base::DoNothing(),  // No special action on test timeout.
      base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
  return rt;
}
