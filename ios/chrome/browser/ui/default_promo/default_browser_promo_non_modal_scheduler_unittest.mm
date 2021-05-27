// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_scheduler.h"

#include "base/ios/ios_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class DefaultBrowserPromoNonModalSchedulerTest : public PlatformTest {
 protected:
  DefaultBrowserPromoNonModalSchedulerTest()
      : web_state_list_(&web_state_list_delegate_) {}
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    std::unique_ptr<TestChromeBrowserState> chrome_browser_state =
        test_cbs_builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state.get(),
                                             &web_state_list_);

    // Add initial web state
    auto web_state = std::make_unique<web::FakeWebState>();
    test_web_state_ = web_state.get();
    web_state_list_.InsertWebState(0, std::move(web_state),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());

    ClearUserDefaults();
  }
  void TearDown() override { ClearUserDefaults(); }

  // Clear NSUserDefault keys used in the class.
  void ClearUserDefaults() {
    NSArray<NSString*>* keys = @[
      @"userInteractedWithNonModalPromoCount",
      @"lastTimeUserInteractedWithFullscreenPromo"
    ];
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    for (NSString* key in keys) {
      [defaults removeObjectForKey:key];
    }
  }

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  web::FakeWebState* test_web_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<Browser> browser_;
};

// Tests that the omnibox paste event triggers the promo to show.
TEST_F(DefaultBrowserPromoNonModalSchedulerTest, TestOmniboxPasteShowsPromo) {
  // Default promo is not supported on iOS versions < 14.
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    return;
  }

  id promo_commands_handler =
      OCMStrictProtocolMock(@protocol(DefaultBrowserPromoNonModalCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:promo_commands_handler
                   forProtocol:@protocol(DefaultBrowserPromoNonModalCommands)];

  DefaultBrowserPromoNonModalScheduler* scheduler =
      [[DefaultBrowserPromoNonModalScheduler alloc] init];
  scheduler.browser = browser_.get();
  scheduler.dispatcher = browser_->GetCommandDispatcher();

  [scheduler logUserPastedInOmnibox];

  // Finish loading the page.
  test_web_state_->SetLoading(true);
  test_web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_->SetLoading(false);

  // Advance the timer by the post-load delay. This should trigger the promo.
  [[promo_commands_handler expect] showDefaultBrowserNonModalPromo];
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(3));

  [promo_commands_handler verify];
}

}  // namespace
