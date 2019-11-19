// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/web_test/web_test_content_renderer_client.h"
#include "content/test/test_content_client.h"
#include "ui/events/platform/platform_event_source.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

#if !defined(OS_CHROMEOS) && defined(OS_LINUX)
#include "ui/base/ime/init/input_method_initializer.h"
#endif

#if defined(OS_CHROMEOS)
#include "content/public/test/network_connection_change_simulator.h"
#endif

#if defined(USE_AURA) && defined(TOOLKIT_VIEWS)
#include "ui/views/test/widget_test_api.h"  // nogncheck
#endif

namespace content {

ContentBrowserTest::ContentBrowserTest() {
#if defined(OS_MACOSX)
  base::mac::SetOverrideAmIBundled(true);

  // See comment in InProcessBrowserTest::InProcessBrowserTest().
  base::FilePath content_shell_path;
  CHECK(base::PathService::Get(base::FILE_EXE, &content_shell_path));
  content_shell_path = content_shell_path.DirName();
  content_shell_path = content_shell_path.Append(
      FILE_PATH_LITERAL("Content Shell.app/Contents/MacOS/Content Shell"));
  CHECK(base::PathService::Override(base::FILE_EXE, content_shell_path));
#endif
  CreateTestServer(GetTestDataFilePath());
}

ContentBrowserTest::~ContentBrowserTest() {
}

void ContentBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);

#if defined(OS_MACOSX)
  // See InProcessBrowserTest::PrepareTestCommandLine().
  base::FilePath subprocess_path;
  base::PathService::Get(base::FILE_EXE, &subprocess_path);
  subprocess_path = subprocess_path.DirName().DirName();
  DCHECK_EQ(subprocess_path.BaseName().value(), "Contents");
  subprocess_path = subprocess_path.Append(
      "Frameworks/Content Shell Framework.framework/Helpers/Content Shell "
      "Helper.app/Contents/MacOS/Content Shell Helper");
  command_line->AppendSwitchPath(switches::kBrowserSubprocessPath,
                                 subprocess_path);
#endif

#if defined(USE_AURA) && defined(TOOLKIT_VIEWS)
  // https://crbug.com/695054: Ignore window activation/deactivation to make
  // the Chrome-internal focus unaffected by OS events caused by running tests
  // in parallel.
  views::DisableActivationChangeHandlingForTests();
#endif

  // LinuxInputMethodContextFactory has to be initialized.
#if !defined(OS_CHROMEOS) && defined(OS_LINUX)
  ui::InitializeInputMethodForTesting();
#endif

  ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);

  BrowserTestBase::SetUp();
}

void ContentBrowserTest::TearDown() {
  BrowserTestBase::TearDown();

  // LinuxInputMethodContextFactory has to be shutdown.
#if !defined(OS_CHROMEOS) && defined(OS_LINUX)
  ui::ShutdownInputMethodForTesting();
#endif
}

void ContentBrowserTest::PreRunTestOnMainThread() {
#if defined(OS_CHROMEOS)
  NetworkConnectionChangeSimulator network_change_simulator;
  network_change_simulator.InitializeChromeosConnectionType();
#endif

  if (!switches::IsRunWebTestsSwitchPresent()) {
    CHECK_EQ(Shell::windows().size(), 1u);
    shell_ = Shell::windows()[0];
    SetInitialWebContents(shell_->web_contents());
  }

#if defined(OS_MACOSX)
  // On Mac, without the following autorelease pool, code which is directly
  // executed (as opposed to executed inside a message loop) would autorelease
  // objects into a higher-level pool. This pool is not recycled in-sync with
  // the message loops' pools and causes problems with code relying on
  // deallocation via an autorelease pool (such as browser window closure and
  // browser shutdown). To avoid this, the following pool is recycled after each
  // time code is directly executed.
  pool_ = new base::mac::ScopedNSAutoreleasePool;
#endif

  // Pump startup related events.
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  base::RunLoop().RunUntilIdle();

#if defined(OS_MACOSX)
  pool_->Recycle();
#endif

  pre_run_test_executed_ = true;
}

void ContentBrowserTest::PostRunTestOnMainThread() {
  // This code is failing when the test is overriding PreRunTestOnMainThread()
  // without the required call to ContentBrowserTest::PreRunTestOnMainThread().
  // This is a common error causing a crash on MAC.
  DCHECK(pre_run_test_executed_);

#if defined(OS_MACOSX)
  pool_->Recycle();
#endif

  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }

  Shell::CloseAllWindows();
}

Shell* ContentBrowserTest::CreateBrowser() {
  return Shell::CreateNewWindow(
      ShellContentBrowserClient::Get()->browser_context(),
      GURL(url::kAboutBlankURL), nullptr, gfx::Size());
}

Shell* ContentBrowserTest::CreateOffTheRecordBrowser() {
  return Shell::CreateNewWindow(
      ShellContentBrowserClient::Get()->off_the_record_browser_context(),
      GURL(url::kAboutBlankURL), nullptr, gfx::Size());
}

base::FilePath ContentBrowserTest::GetTestDataFilePath() {
  return base::FilePath(FILE_PATH_LITERAL("content/test/data"));
}

}  // namespace content
