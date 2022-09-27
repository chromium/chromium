// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/blink_perf_test_suite.h"

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/test/perf_log.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

BlinkPerfTestSuite::BlinkPerfTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv), env(argc, argv) {}

void BlinkPerfTestSuite::Initialize() {
  TestSuite::Initialize();

  // Initialize the perf timer log
  base::FilePath log_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath("log-file");
  if (log_path.empty()) {
    base::PathService::Get(base::FILE_EXE, &log_path);
#if BUILDFLAG(IS_ANDROID)
    base::FilePath tmp_dir;
    base::PathService::Get(base::DIR_CACHE, &tmp_dir);
    log_path = tmp_dir.Append(log_path.BaseName());
#endif
    log_path = log_path.ReplaceExtension(FILE_PATH_LITERAL("log"));
    log_path = log_path.InsertBeforeExtension(FILE_PATH_LITERAL("_perf"));
  }
  ASSERT_TRUE(base::InitPerfLog(log_path));

  // Raise to high priority to have more precise measurements. Since we don't
  // aim at 1% precision, it is not necessary to run at realtime level.
  if (!base::debug::BeingDebugged())
    base::RaiseProcessToHighPriority();
}

void BlinkPerfTestSuite::Shutdown() {
  TestSuite::Shutdown();
  base::FinalizePerfLog();
}

}  // namespace blink
