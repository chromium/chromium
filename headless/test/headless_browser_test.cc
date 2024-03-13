// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_browser_test.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
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
#include "headless/public/headless_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/v8_context_snapshot/buildflags.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "services/device/public/cpp/test/fake_geolocation_system_permission_manager.h"
#endif

namespace headless {

HeadlessBrowserTest::HeadlessBrowserTest() {
#if BUILDFLAG(IS_MAC)
  // On Mac the source root is not set properly. We override it by assuming
  // that is two directories up from the execution test file.
  base::FilePath dir_exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &dir_exe_path));
  dir_exe_path = dir_exe_path.Append("../../");
  CHECK(
      base::PathService::Override(base::DIR_SRC_TEST_DATA_ROOT, dir_exe_path));
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
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
  constexpr gin::V8SnapshotFileType kSnapshotType =
      gin::V8SnapshotFileType::kWithAdditionalContext;
#else
  constexpr gin::V8SnapshotFileType kSnapshotType =
      gin::V8SnapshotFileType::kDefault;
#endif  // BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
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
  auto fake_geolocation_system_permission_manager =
      std::make_unique<device::FakeGeolocationSystemPermissionManager>();
  fake_geolocation_system_permission_manager->SetSystemPermission(
      device::LocationSystemPermissionStatus::kAllowed);
  static_cast<HeadlessBrowserImpl*>(browser())
      ->SetGeolocationSystemPermissionManagerForTesting(
          std::move(fake_geolocation_system_permission_manager));
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

}  // namespace headless
