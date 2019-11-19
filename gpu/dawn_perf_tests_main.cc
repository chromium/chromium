// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"

namespace {

int RunHelper(base::TestSuite* test_suite) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  return test_suite->Run();
}

}  // namespace

// Definition located in third_party/dawn/src/tests/perf_tests/DawnPerfTest.h
// Forward declared here to avoid pulling in the Dawn headers.
void InitDawnPerfTestEnvironment(int argc, char** argv);

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  InitDawnPerfTestEnvironment(argc, argv);

  base::TestSuite test_suite(argc, argv);
  int rt = base::LaunchUnitTestsSerially(
      argc, argv, base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
  return rt;
}
