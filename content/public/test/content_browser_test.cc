// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/test_browser_context.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_paths.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/test_content_client.h"
#include "ui/events/platform/platform_event_source.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "content/shell/app/paths_mac.h"
#endif

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#include "ui/base/ime/init/input_method_initializer.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "content/public/test/network_connection_change_simulator.h"
#endif

#if defined(USE_AURA) && defined(TOOLKIT_VIEWS)
#include "ui/views/test/widget_test_api.h"  // nogncheck
#endif

namespace content {

ContentBrowserTest::ContentBrowserTest() {
  // In content browser tests ContentBrowserTestContentBrowserClient must be
  // used. ContentBrowserTestContentBrowserClient's constructor (and destructor)
  // uses this same function to change the ContentBrowserClient.
  ContentClient::SetCanChangeContentBrowserClientForTesting(false);
#if BUILDFLAG(IS_MAC)
  base::apple::SetOverrideAmIBundled(true);

  // See comment in InProcessBrowserTest::InProcessBrowserTest().
  base::FilePath content_shell_path;
  CHECK(base::PathService::Get(base::FILE_EXE, &content_shell_path));
  content_shell_path = content_shell_path.DirName();
  content_shell_path = content_shell_path.Append(
      FILE_PATH_LITERAL("Content Shell.app/Contents/MacOS/Content Shell"));
  CHECK(base::CreateDirectory(content_shell_path.DirName()));
  if (base::File file(content_shell_path,
                      base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
      !file.IsValid()) {
    // Diagnostics for https://crbug.com/345765743.
    const auto last_errno = errno;
    CHECK(base::PathExists(content_shell_path))
        << "Failed to create \"" << content_shell_path
        << "\": " << base::File::ErrorToString(file.error_details())
        << "; errno = " << last_errno;
  }
  file_exe_override_.emplace(base::FILE_EXE, content_shell_path,
                             /*is_absolute=*/false, /*create=*/false);
#endif

  // The HTTPS test server must be setup here as different browser test suites
  // have different bundle behavior on macOS, and the HTTPS test server
  // constructor reads in the local test root cert. It might be possible
  // to move this to BrowserTestBase in the future.
  embedded_https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  // Default hostnames for the HTTPS test server. Test fixtures can call this
  // with different hostnames (before starting the server) to override.
  embedded_https_test_server_->SetCertHostnames(
      {"example.com", "*.example.com", "foo.com", "*.foo.com", "bar.com",
       "*.bar.com", "a.com", "*.a.com", "b.com", "*.b.com", "c.com",
       "*.c.com"});

  embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
  embedded_https_test_server().AddDefaultHandlers(GetTestDataFilePath());
}

ContentBrowserTest::~ContentBrowserTest() {
}

void ContentBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);

#if BUILDFLAG(IS_MAC)
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

  // Needs to happen before ContentMain().
  OverrideFrameworkBundlePath();
  OverrideOuterBundlePath();
  OverrideChildProcessPath();
  OverrideSourceRootPath();
  OverrideBundleID();
#endif

#if defined(USE_AURA) && defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_CASTOS)
  // https://crbug.com/695054: Ignore window activation/deactivation to make
  // the Chrome-internal focus unaffected by OS events caused by running tests
  // in parallel.
  views::DisableActivationChangeHandlingForTests();
#endif

  // LinuxInputMethodContextFactory has to be initialized.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  ui::InitializeInputMethodForTesting();
#endif

  ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);

// Enable this switch to prevent undesired viewport resizing for the scaling
// issue addressed in https://crrev.com/c/4615623.
#if BUILDFLAG(IS_IOS)
  command_line->AppendSwitch(switches::kPreventResizingContentsForTesting);
#endif

  BrowserTestBase::SetUp();
}

void ContentBrowserTest::TearDown() {
  BrowserTestBase::TearDown();

  if (embedded_https_test_server().Started()) {
    ASSERT_TRUE(embedded_https_test_server().ShutdownAndWaitUntilComplete());
  }

  // LinuxInputMethodContextFactory has to be shutdown.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  ui::ShutdownInputMethodForTesting();
#endif
}

void ContentBrowserTest::PreRunTestOnMainThread() {
#if BUILDFLAG(IS_CHROMEOS)
  NetworkConnectionChangeSimulator network_change_simulator;
  network_change_simulator.InitializeChromeosConnectionType();
#endif

  CHECK_EQ(Shell::windows().size(), 1u);
  shell_ = Shell::windows()[0];
  SetInitialWebContents(shell_->web_contents());

#if BUILDFLAG(IS_MAC)
  // On Mac, without the following autorelease pool, code which is directly
  // executed (as opposed to executed inside a message loop) would autorelease
  // objects into a higher-level pool. This pool is not recycled in-sync with
  // the message loops' pools and causes problems with code relying on
  // deallocation via an autorelease pool (such as browser window closure and
  // browser shutdown). To avoid this, the following pool is recycled after each
  // time code is directly executed.
  pool_.emplace();
#endif

  // Pump startup related events.
  DCHECK(base::CurrentUIThread::IsSet());
  base::RunLoop().RunUntilIdle();

#if BUILDFLAG(IS_MAC)
  pool_->Recycle();
#endif

  pre_run_test_executed_ = true;
}

void ContentBrowserTest::PostRunTestOnMainThread() {
  // This code is failing when the test is overriding PreRunTestOnMainThread()
  // without the required call to ContentBrowserTest::PreRunTestOnMainThread().
  // This is a common error causing a crash on MAC.
  DCHECK(pre_run_test_executed_);

#if BUILDFLAG(IS_MAC)
  pool_.reset();
#endif

  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }

  Shell::Shutdown();
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

std::unique_ptr<TestBrowserContext>
ContentBrowserTest::CreateTestBrowserContext() {
  base::FilePath user_data_path;
  EXPECT_TRUE(base::PathService::Get(SHELL_DIR_USER_DATA, &user_data_path));
  base::FilePath browser_context_dir_path;
  EXPECT_TRUE(base::CreateTemporaryDirInDir(
      /*base_dir=*/user_data_path,
      /*prefix=*/FILE_PATH_LITERAL("test_browser_context_"),
      /*new_dir=*/&browser_context_dir_path));
  return std::make_unique<TestBrowserContext>(browser_context_dir_path);
}

base::FilePath ContentBrowserTest::GetTestDataFilePath() {
  return base::FilePath(FILE_PATH_LITERAL("content/test/data"));
}

}  // namespace content
