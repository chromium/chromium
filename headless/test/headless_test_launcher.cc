// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/launcher/test_launcher.h"
#include "build/build_config.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/public/test/test_launcher.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace headless {
namespace {

class HeadlessTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  HeadlessTestLauncherDelegate() = default;

  HeadlessTestLauncherDelegate(const HeadlessTestLauncherDelegate&) = delete;
  HeadlessTestLauncherDelegate& operator=(const HeadlessTestLauncherDelegate&) =
      delete;

  ~HeadlessTestLauncherDelegate() override = default;

  // content::TestLauncherDelegate implementation:
  int RunTestSuite(int argc, char** argv) override {
    base::TestSuite test_suite(argc, argv);
    // Browser tests are expected not to tear-down various globals.
    test_suite.DisableCheckForLeakedGlobals();
    return test_suite.Run();
  }

 protected:
  content::ContentMainDelegate* CreateContentMainDelegate() override {
    return new HeadlessContentMainDelegate(
        std::make_unique<HeadlessBrowserImpl>(base::DoNothing()));
  }
};

}  // namespace
}  // namespace headless

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs(/*cores_per_job=*/2);
  if (parallel_jobs == 0U)
    return 1;

#if BUILDFLAG(IS_WIN)
  // Load and pin user32.dll to avoid having to load it once tests start while
  // on the main thread loop where blocking calls are disallowed.
  base::win::PinUser32();
#endif  // BUILDFLAG(IS_WIN)

  // Setup a working test environment for the network service in case it's used.
  // Only create this object in the utility process, so that its members don't
  // interfere with other test objects in the browser process.
  std::unique_ptr<content::NetworkServiceTestHelper>
      network_service_test_helper = content::NetworkServiceTestHelper::Create();

  headless::HeadlessTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
