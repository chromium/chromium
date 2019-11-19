// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_suite.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_launcher.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "fuchsia/engine/web_engine_main_delegate.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

class WebEngineTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  WebEngineTestLauncherDelegate() = default;
  ~WebEngineTestLauncherDelegate() override = default;

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

  content::ContentMainDelegate* CreateContentMainDelegate() override {
    // Set up the channels for the Context service, but postpone client
    // binding until after the browser TaskRunners are up and running.
    fidl::InterfaceHandle<fuchsia::web::Context> context;
    content::ContentMainDelegate* content_main_delegate =
        new WebEngineMainDelegate(context.NewRequest());

    cr_fuchsia::WebEngineBrowserTest::SetContextClientChannel(
        context.TakeChannel());

    return content_main_delegate;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebEngineTestLauncherDelegate);
};

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kOzonePlatform, "headless");
  command_line->AppendSwitchASCII(switches::kEnableLogging, "stderr");
  command_line->AppendSwitch(switches::kDisableGpu);

  size_t parallel_jobs = base::NumParallelJobs();
  if (parallel_jobs > 1U) {
    parallel_jobs /= 2U;
  }
  ::WebEngineTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
