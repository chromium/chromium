// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/process/memory.h"
#include "base/system/sys_info.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/ios_buildflags.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test_shell_main_delegate.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_launcher.h"
#include "content/shell/common/shell_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/buildflags.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

class ContentBrowserTestSuite : public ContentTestSuiteBase {
 public:
  ContentBrowserTestSuite(int argc, char** argv)
      : ContentTestSuiteBase(argc, argv) {
  }

  ContentBrowserTestSuite(const ContentBrowserTestSuite&) = delete;
  ContentBrowserTestSuite& operator=(const ContentBrowserTestSuite&) = delete;

  ~ContentBrowserTestSuite() override {}

 protected:
  void Initialize() override {
    // Browser tests are expected not to tear-down various globals.
    base::TestSuite::DisableCheckForLeakedGlobals();

    ContentTestSuiteBase::Initialize();

#if BUILDFLAG(IS_ANDROID)
    RegisterInProcessThreads();
#endif
  }
};

class ContentTestLauncherDelegate : public TestLauncherDelegate {
 public:
  ContentTestLauncherDelegate() {}

  ContentTestLauncherDelegate(const ContentTestLauncherDelegate&) = delete;
  ContentTestLauncherDelegate& operator=(const ContentTestLauncherDelegate&) =
      delete;

  ~ContentTestLauncherDelegate() override {}

  int RunTestSuite(int argc, char** argv) override {
    return ContentBrowserTestSuite(argc, argv).Run();
  }

  std::string GetUserDataDirectoryCommandLineSwitch() override {
    return switches::kContentShellUserDataDir;
  }

 protected:
#if !BUILDFLAG(IS_ANDROID)
  ContentMainDelegate* CreateContentMainDelegate() override {
    return new ContentBrowserTestShellMainDelegate();
  }
#endif
};

}  // namespace content

#if BUILDFLAG(IS_IOS) && BUILDFLAG(IS_IOS_APP_EXTENSION)
extern "C" int ChildProcessMain(int argc, const char** argv)
#else
int main(int argc, char** argv)
#endif
{
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs(/*cores_per_job=*/2);
  if (parallel_jobs == 0U)
    return 1;

#if BUILDFLAG(IS_WIN)
  // Load and pin user32.dll to avoid having to load it once tests start while
  // on the main thread loop where blocking calls are disallowed.
  base::win::PinUser32();
#endif  // BUILDFLAG(IS_WIN)
  content::ContentTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, parallel_jobs, argc,
                     const_cast<char**>(argv));
}
