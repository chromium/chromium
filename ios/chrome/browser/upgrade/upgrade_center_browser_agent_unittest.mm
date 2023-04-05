// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/upgrade_center_browser_agent.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/upgrade/test/fake_upgrade_center.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class UpgradeCenterBrowserAgentTest : public PlatformTest {
 public:
  UpgradeCenterBrowserAgentTest() {}
  ~UpgradeCenterBrowserAgentTest() override {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    fake_upgrade_center_ = [[FakeUpgradeCenter alloc] init];
    UpgradeCenterBrowserAgent::CreateForBrowser(browser_.get(),
                                                fake_upgrade_center_);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  FakeUpgradeCenter* fake_upgrade_center_;
};

TEST_F(UpgradeCenterBrowserAgentTest, AddsInfoBarManagerOnWebStateInsert) {
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  NSString* stable_identifier = fake_web_state->GetStableIdentifier();

  browser_->GetWebStateList()->InsertWebState(
      WebStateList::kInvalidIndex, std::move(fake_web_state),
      WebStateList::INSERT_NO_FLAGS, WebStateOpener());

  ASSERT_NE(nullptr, fake_upgrade_center_.infoBarManagers[stable_identifier]);
  ASSERT_EQ(fake_upgrade_center_.infoBarManagers.count, 1U);
}

TEST_F(UpgradeCenterBrowserAgentTest, RemovesInfoBarManagerOnWebStateDetach) {
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  NSString* stable_identifier = fake_web_state->GetStableIdentifier();

  browser_->GetWebStateList()->InsertWebState(
      WebStateList::kInvalidIndex, std::move(fake_web_state),
      WebStateList::INSERT_NO_FLAGS, WebStateOpener());

  ASSERT_NE(nullptr, fake_upgrade_center_.infoBarManagers[stable_identifier]);
  ASSERT_EQ(fake_upgrade_center_.infoBarManagers.count, 1U);

  browser_->GetWebStateList()->DetachWebStateAt(0);

  ASSERT_EQ(nullptr, fake_upgrade_center_.infoBarManagers[stable_identifier]);
  ASSERT_EQ(fake_upgrade_center_.infoBarManagers.count, 0U);
}
