// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_test.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_system.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_extension_system.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "content/public/test/network_connection_change_simulator.h"
#endif

namespace extensions {

AppShellTest::AppShellTest() {
  CreateTestServer(base::FilePath(FILE_PATH_LITERAL("extensions/test/data")));
}

AppShellTest::~AppShellTest() = default;

void AppShellTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kTestType, "appshell");
  SetUpCommandLine(command_line);
  content::BrowserTestBase::SetUp();
}

void AppShellTest::PreRunTestOnMainThread() {
#if BUILDFLAG(IS_CHROMEOS)
  content::NetworkConnectionChangeSimulator network_change_simulator;
  network_change_simulator.InitializeChromeosConnectionType();
#endif

  browser_context_ = ShellContentBrowserClient::Get()->GetBrowserContext();

  extension_system_ = static_cast<ShellExtensionSystem*>(
      ExtensionSystem::Get(browser_context_));
  extension_system_->FinishInitialization();
  DCHECK(base::CurrentUIThread::IsSet());
  base::RunLoop().RunUntilIdle();
}

void AppShellTest::PostRunTestOnMainThread() {
  // Clean up the app window.
  DesktopController::instance()->CloseAppWindows();
}

}  // namespace extensions
