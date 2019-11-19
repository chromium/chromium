// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/launcher/test_launcher.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/public/test/test_launcher.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "headless/lib/utility/headless_content_utility_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {
namespace {

class HeadlessBrowserImplForTest : public HeadlessBrowserImpl {
 public:
  explicit HeadlessBrowserImplForTest(HeadlessBrowser::Options options)
      : HeadlessBrowserImpl(base::BindOnce(&HeadlessBrowserImplForTest::OnStart,
                                           base::Unretained(this)),
                            std::move(options)) {}

  void OnStart(HeadlessBrowser* browser) { EXPECT_EQ(this, browser); }

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserImplForTest);
};

class HeadlessTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  HeadlessTestLauncherDelegate() = default;
  ~HeadlessTestLauncherDelegate() override = default;

  // content::TestLauncherDelegate implementation:
  int RunTestSuite(int argc, char** argv) override {
    base::TestSuite test_suite(argc, argv);
    // Browser tests are expected not to tear-down various globals and may
    // complete with the thread priority being above NORMAL.
    test_suite.DisableCheckForLeakedGlobals();
    test_suite.DisableCheckForThreadPriorityAtTestEnd();
    return test_suite.Run();
  }

  bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) override {
    return true;
  }

 protected:
  content::ContentMainDelegate* CreateContentMainDelegate() override {
    // Use HeadlessBrowserTest::options() or HeadlessBrowserContextOptions to
    // modify these defaults.
    HeadlessBrowser::Options::Builder options_builder;
    std::unique_ptr<HeadlessBrowserImpl> browser(
        new HeadlessBrowserImplForTest(options_builder.Build()));
    return new HeadlessContentMainDelegate(std::move(browser));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessTestLauncherDelegate);
};

}  // namespace
}  // namespace headless

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs();
  if (parallel_jobs > 1U) {
    parallel_jobs /= 2U;
  }

  // Setup a working test environment for the network service in case it's used.
  // Only create this object in the utility process, so that its members don't
  // interfere with other test objects in the browser process.
  std::unique_ptr<content::NetworkServiceTestHelper>
      network_service_test_helper;
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType) == switches::kUtilityProcess) {
    network_service_test_helper =
        std::make_unique<content::NetworkServiceTestHelper>();
    headless::HeadlessContentUtilityClient::
        SetNetworkBinderCreationCallbackForTests(base::BindRepeating(
            [](content::NetworkServiceTestHelper* helper,
               service_manager::BinderRegistry* registry) {
              helper->RegisterNetworkBinders(registry);
            },
            network_service_test_helper.get()));
  }

  headless::HeadlessTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
