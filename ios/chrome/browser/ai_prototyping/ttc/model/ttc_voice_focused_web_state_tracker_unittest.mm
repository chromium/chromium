// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_voice_focused_web_state_tracker.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <utility>

#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for `TtcVoiceFocusedWebStateTracker`.
class TtcVoiceFocusedWebStateTrackerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    first_scene_state_ = [[SceneState alloc] init];
    first_scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    first_browser_ =
        std::make_unique<TestBrowser>(profile_.get(), first_scene_state_);
    browser_list_->AddBrowser(first_browser_.get());
  }

  void TearDown() override {
    if (first_browser_) {
      browser_list_->RemoveBrowser(first_browser_.get());
      first_browser_.reset();
    }
    first_scene_state_ = nil;
    browser_list_ = nullptr;
    profile_.reset();
    PlatformTest::TearDown();
  }

  // Inserts and activates a fake tab in `browser`, returning a non-owning
  // pointer to it.
  web::FakeWebState* InsertActiveTab(Browser* browser) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* ptr = web_state.get();
    browser->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return ptr;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<BrowserList> browser_list_ = nullptr;
  SceneState* first_scene_state_ = nil;
  std::unique_ptr<TestBrowser> first_browser_;
};

// Tests that the tracker synchronously binds to the initial active tab on
// construction.
TEST_F(TtcVoiceFocusedWebStateTrackerTest, InitialActiveTab) {
  web::FakeWebState* tab = InsertActiveTab(first_browser_.get());
  TtcVoiceFocusedWebStateTracker tracker(browser_list_);

  EXPECT_EQ(tracker.GetActiveWebState(), tab);
}

// Tests that `Disconnect()` clears the tracked `WebState` and that a
// `nullptr` `BrowserList` is safe.
TEST_F(TtcVoiceFocusedWebStateTrackerTest, DisconnectClearsTrackedWebState) {
  TtcVoiceFocusedWebStateTracker null_tracker(nullptr);
  EXPECT_EQ(null_tracker.GetActiveWebState(), nullptr);

  web::FakeWebState* first_tab = InsertActiveTab(first_browser_.get());
  TtcVoiceFocusedWebStateTracker tracker(browser_list_);
  EXPECT_EQ(tracker.GetActiveWebState(), first_tab);

  tracker.Disconnect();
  EXPECT_EQ(tracker.GetActiveWebState(), nullptr);
}
