// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#include "ios/chrome/browser/test/perf_test_with_bvc_ios.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const NSTimeInterval kMaxUICatchupDelay = 2.0;  // seconds

class NewTabPagePerfTest : public PerfTestWithBVC {
 public:
  NewTabPagePerfTest()
      : PerfTestWithBVC("NTP - Create",
                        "First Tab",
                        "",
                        false,
                        false,
                        true,
                        10) {}

 protected:
  void SetUp() override {
    PerfTestWithBVC::SetUp();
    IOSChromeTabRestoreServiceFactory::GetInstance()->SetTestingFactory(
        chrome_browser_state_.get(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
  }
  base::TimeDelta TimedNewTab() {
    base::Time startTime = base::Time::NowFromSystemTime();
    [[bvc_ dispatcher] openURLInNewTab:[OpenNewTabCommand command]];
    return base::Time::NowFromSystemTime() - startTime;
  }
  void SettleUI() {
    base::test::ios::WaitUntilCondition(
        nil, false, base::TimeDelta::FromSecondsD(kMaxUICatchupDelay));
  }
};

// Output format first test:
// [*]RESULT NTP - Create: NTP Gentle Create First Tab= number ms
// Output format subsequent average:
// [*]RESULT NTP - Create: NTP Gentle Create= number ms
// TODO(crbug.com/717314): Failed DCHECK in PerfTestWithBVC::SetUp().
TEST_F(NewTabPagePerfTest, DISABLED_OpenNTP_Gentle) {
  RepeatTimedRuns("NTP Gentle Create",
                  ^(int index) {
                    return TimedNewTab();
                  },
                  ^{
                    SettleUI();
                  });
}

// Output format first test:
// [*]RESULT NTP - Create: NTP Hammer Create First Tab= number ms
// Output format subsequent average:
// [*]RESULT NTP - Create: NTP Hammer Create= number ms
// TODO(crbug.com/717314): Failed DCHECK in PerfTestWithBVC::SetUp().
TEST_F(NewTabPagePerfTest, DISABLED_OpenNTP_Hammer) {
  RepeatTimedRuns("NTP Hammer Create",
                  ^(int index) {
                    return TimedNewTab();
                  },
                  nil);
  // Allows the run loops to run before teardown.
  SettleUI();
}

}  // anonymous namespace
