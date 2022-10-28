// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_browser_test.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "gin/v8_initializer.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "services/device/public/cpp/test/fake_geolocation_manager.h"
#endif

namespace headless {

HeadlessBrowserTest::HeadlessBrowserTest() {
#if BUILDFLAG(IS_MAC)
  // On Mac the source root is not set properly. We override it by assuming
  // that is two directories up from the execution test file.
  base::FilePath dir_exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &dir_exe_path));
  dir_exe_path = dir_exe_path.Append("../../");
  CHECK(base::PathService::Override(base::DIR_SOURCE_ROOT, dir_exe_path));
#endif  // BUILDFLAG(IS_MAC)
  base::FilePath headless_test_data(FILE_PATH_LITERAL("headless/test/data"));
  CreateTestServer(headless_test_data);
}

void HeadlessBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Enable GPU usage (i.e., SwiftShader, hardware GL on macOS) in all tests
  // since that's the default configuration of --headless.
  command_line->AppendSwitch(switches::kUseGpuInTests);
  SetUpCommandLine(command_line);
  BrowserTestBase::SetUp();
}

void HeadlessBrowserTest::SetUpWithoutGPU() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);
  BrowserTestBase::SetUp();
}

HeadlessBrowserTest::~HeadlessBrowserTest() = default;

void HeadlessBrowserTest::PreRunTestOnMainThread() {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
  constexpr gin::V8SnapshotFileType kSnapshotType =
      gin::V8SnapshotFileType::kWithAdditionalContext;
#else
  constexpr gin::V8SnapshotFileType kSnapshotType =
      gin::V8SnapshotFileType::kDefault;
#endif  // USE_V8_CONTEXT_SNAPSHOT
  gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
#endif

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Pump startup related events.
  base::RunLoop().RunUntilIdle();
}

void HeadlessBrowserTest::PostRunTestOnMainThread() {
  browser()->Shutdown();
  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }
  // Pump tasks produced during shutdown.
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(IS_MAC)
void HeadlessBrowserTest::CreatedBrowserMainParts(
    content::BrowserMainParts* parts) {
  auto fake_geolocation_manager =
      std::make_unique<device::FakeGeolocationManager>();
  fake_geolocation_manager->SetSystemPermission(
      device::LocationSystemPermissionStatus::kAllowed);
  static_cast<HeadlessBrowserMainParts*>(parts)
      ->SetGeolocationManagerForTesting(std::move(fake_geolocation_manager));
}
#endif

HeadlessBrowser* HeadlessBrowserTest::browser() const {
  return HeadlessContentMainDelegate::GetInstance()->browser();
}

HeadlessBrowser::Options* HeadlessBrowserTest::options() const {
  return HeadlessContentMainDelegate::GetInstance()->browser()->options();
}

void HeadlessBrowserTest::RunAsynchronousTest() {
  EXPECT_FALSE(run_loop_);
  run_loop_ = std::make_unique<base::RunLoop>(
      base::RunLoop::Type::kNestableTasksAllowed);
  run_loop_->Run();
  run_loop_ = nullptr;
}

void HeadlessBrowserTest::FinishAsynchronousTest() {
  run_loop_->Quit();
}

HeadlessAsyncDevTooledBrowserTest::HeadlessAsyncDevTooledBrowserTest()
    : browser_context_(nullptr),
      web_contents_(nullptr),
      render_process_exited_(false) {}

HeadlessAsyncDevTooledBrowserTest::~HeadlessAsyncDevTooledBrowserTest() =
    default;

void HeadlessAsyncDevTooledBrowserTest::DevToolsTargetReady() {
  EXPECT_TRUE(web_contents_->GetDevToolsTarget());
  web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
#if BUILDFLAG(IS_MAC)
  devtools_client_->GetEmulation()->SetDeviceMetricsOverride(
      emulation::SetDeviceMetricsOverrideParams::Builder()
          .SetWidth(0)
          .SetHeight(0)
          .SetDeviceScaleFactor(1)
          .SetMobile(false)
          .Build(),
      base::BindOnce(
          [](HeadlessAsyncDevTooledBrowserTest* self) {
            self->RunDevTooledTest();
          },
          base::Unretained(this)));
#else
  RunDevTooledTest();
#endif
}

void HeadlessAsyncDevTooledBrowserTest::RenderProcessExited(
    base::TerminationStatus status,
    int exit_code) {
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION)
    return;

  FinishAsynchronousTest();
  render_process_exited_ = true;
  FAIL() << "Abnormal renderer termination "
         << "(status=" << status << ", exit_code=" << exit_code << ")";
}

void HeadlessAsyncDevTooledBrowserTest::RunTest() {
  devtools_client_ = HeadlessDevToolsClient::Create();
  browser_devtools_client_ = HeadlessDevToolsClient::Create();
  interceptor_ = std::make_unique<TestNetworkInterceptor>();
  HeadlessBrowserContext::Builder builder =
      browser()->CreateBrowserContextBuilder();
  CustomizeHeadlessBrowserContext(builder);
  browser_context_ = builder.Build();

  browser()->SetDefaultBrowserContext(browser_context_);
  browser()->GetDevToolsTarget()->AttachClient(browser_devtools_client_.get());

  HeadlessWebContents::Builder web_contents_builder =
      browser_context_->CreateWebContentsBuilder();
  web_contents_builder.SetEnableBeginFrameControl(GetEnableBeginFrameControl());
  CustomizeHeadlessWebContents(web_contents_builder);
  web_contents_ = web_contents_builder.Build();

  web_contents_->AddObserver(this);

  RunAsynchronousTest();
  interceptor_.reset();
  if (!render_process_exited_)
    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  web_contents_->RemoveObserver(this);
  web_contents_->Close();
  web_contents_ = nullptr;
  browser()->GetDevToolsTarget()->DetachClient(browser_devtools_client_.get());
  browser_context_->Close();
  browser_context_ = nullptr;
  // Let the tasks that might have beein scheduled during web contents
  // being closed run (see https://crbug.com/1036627 for details).
  base::RunLoop().RunUntilIdle();
}

bool HeadlessAsyncDevTooledBrowserTest::GetEnableBeginFrameControl() {
  return false;
}

void HeadlessAsyncDevTooledBrowserTest::CustomizeHeadlessBrowserContext(
    HeadlessBrowserContext::Builder& builder) {}

void HeadlessAsyncDevTooledBrowserTest::CustomizeHeadlessWebContents(
    HeadlessWebContents::Builder& builder) {}

}  // namespace headless
