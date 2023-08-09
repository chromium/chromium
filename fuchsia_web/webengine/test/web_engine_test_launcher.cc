// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>
#include <utility>

#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_suite.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_launcher.h"
#include "fuchsia_web/webengine/browser/web_engine_browser_main_parts.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "fuchsia_web/webengine/web_engine_main_delegate.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

class WebEngineTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  WebEngineTestLauncherDelegate() = default;
  ~WebEngineTestLauncherDelegate() override = default;

  WebEngineTestLauncherDelegate(const WebEngineTestLauncherDelegate&) = delete;
  WebEngineTestLauncherDelegate& operator=(
      const WebEngineTestLauncherDelegate&) = delete;

  // content::TestLauncherDelegate implementation:
  int RunTestSuite(int argc, char** argv) override {
    base::TestSuite test_suite(argc, argv);
    // Browser tests are expected not to tear-down various globals.
    test_suite.DisableCheckForLeakedGlobals();
    return test_suite.Run();
  }

  content::ContentMainDelegate* CreateContentMainDelegate() override {
    return new WebEngineMainDelegate();
  }
};

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kEnableLogging, "stderr");

  // Indicate to all processes that they are being run as part of a browser
  // test, so that dependencies which might compromise test isolation
  // won't be used (e.g. memory pressure).
  command_line->AppendSwitch(switches::kBrowserTest);

  size_t parallel_jobs = base::NumParallelJobs(/*cores_per_job=*/2);
  if (parallel_jobs == 0U)
    return 1;
  ::WebEngineTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
