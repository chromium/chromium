// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/angle/include/GLSLANG/ShaderLang.h"

namespace {

int RunHelper(base::TestSuite* test_suite) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  return test_suite->Run();
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  testing::InitGoogleMock(&argc, argv);
  sh::Initialize();

  base::TestSuite test_suite(argc, argv);
  test_suite.DisableCheckForThreadAndProcessPriority();

  int rt = base::LaunchUnitTestsSerially(
      argc, argv, base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
  sh::Finalize();
  return rt;
}
