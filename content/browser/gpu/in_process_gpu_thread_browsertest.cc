// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"

namespace {

using content::InProcessGpuThread;
using content::GpuProcessHost;

class InProcessGpuTest : public content::ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kInProcessGPU);
    content::ContentBrowserTest::SetUpCommandLine(command_line);
  }
};

// Reproduces the race that could give crbug.com/799002's "hang until OOM" at
// shutdown.
IN_PROC_BROWSER_TEST_F(InProcessGpuTest, NoHangAtQuickLaunchAndShutDown) {
  // ... then exit the browser.
}

// Tests crbug.com/799002 but with another timing.
IN_PROC_BROWSER_TEST_F(InProcessGpuTest, NoCrashAtShutdown) {
  GpuProcessHost::Get();
  // ... then exit the browser.
}

}  // namespace
