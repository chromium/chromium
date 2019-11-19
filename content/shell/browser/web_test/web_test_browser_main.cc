// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_browser_main.h"

#include <iostream>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/url_constants.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/web_test/blink_test_controller.h"
#include "content/shell/browser/web_test/test_info_extractor.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/shell/renderer/web_test/blink_test_helpers.h"
#include "gpu/config/gpu_switches.h"
#include "net/base/filename_util.h"

namespace {

bool RunOneTest(const content::TestInfo& test_info,
                content::BlinkTestController* blink_test_controller,
                content::BrowserMainRunner* main_runner) {
  DCHECK(blink_test_controller);

  if (!blink_test_controller->PrepareForWebTest(test_info))
    return false;

  main_runner->Run();

  return blink_test_controller->ResetAfterWebTest();
}

void RunTests(content::BrowserMainRunner* main_runner) {
  content::BlinkTestController test_controller;
  {
    // We're outside of the message loop here, and this is a test.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath temp_path;
    base::GetTempDir(&temp_path);
    test_controller.SetTempPath(temp_path);
  }

  std::cout << "#READY\n";
  std::cout.flush();

  content::TestInfoExtractor test_extractor(
      *base::CommandLine::ForCurrentProcess());
  bool ran_at_least_once = false;
  std::unique_ptr<content::TestInfo> test_info;
  while ((test_info = test_extractor.GetNextTest())) {
    ran_at_least_once = true;
    if (!RunOneTest(*test_info, &test_controller, main_runner))
      break;
  }
  if (!ran_at_least_once) {
    // CloseAllWindows will cause the |main_runner| loop to quit.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&content::Shell::CloseAllWindows));
    main_runner->Run();
  }
}

}  // namespace

// Main routine for running as the Browser process.
void WebTestBrowserMain(const content::MainFunctionParams& parameters) {
  std::unique_ptr<content::BrowserMainRunner> main_runner =
      content::BrowserMainRunner::Create();

  base::ScopedTempDir browser_context_path_for_web_tests;

  CHECK(browser_context_path_for_web_tests.CreateUniqueTempDir());
  CHECK(!browser_context_path_for_web_tests.GetPath().MaybeAsASCII().empty());
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kIgnoreCertificateErrors);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kContentShellDataPath,
      browser_context_path_for_web_tests.GetPath().MaybeAsASCII());

  // Always disable the unsandbox GPU process for DX12 and Vulkan Info
  // collection to avoid interference. This GPU process is launched 120
  // seconds after chrome starts.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableGpuProcessForDX12VulkanInfoCollection);

  int initialize_exit_code = main_runner->Initialize(parameters);
  DCHECK_LT(initialize_exit_code, 0)
      << "BrowserMainRunner::Initialize failed in WebTestBrowserMain";

  RunTests(main_runner.get());
  base::RunLoop().RunUntilIdle();

  content::Shell::CloseAllWindows();

  main_runner->Shutdown();
}
