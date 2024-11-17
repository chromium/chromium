// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_help_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
// Create the Feature Engagement Mock Tracker.
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    web::BrowserState* browser_state) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

class PopupMenuHelpCoordinatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    ClearDefaultBrowserPromoData();

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));

    profile_ = std::move(builder).Build();

    AppState* app_state = [[AppState alloc] initWithStartupInformation:nil];
    scene_state_ = [[SceneState alloc] initWithAppState:app_state];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    UIViewController* root_view_controller = [[UIViewController alloc] init];
    popup_menu_help_coordinator_ = [[PopupMenuHelpCoordinator alloc]
        initWithBaseViewController:root_view_controller
                           browser:browser_.get()];
    popupMenuUIUpdating_ = OCMProtocolMock(@protocol(PopupMenuUIUpdating));
    popup_menu_help_coordinator_.UIUpdater = popupMenuUIUpdating_;
    [popup_menu_help_coordinator_ start];

    tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));
    ON_CALL(*tracker_, IsInitialized()).WillByDefault(testing::Return(true));
  }

  void TearDown() override {
    ClearDefaultBrowserPromoData();
    profile_.reset();
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  PopupMenuHelpCoordinator* popup_menu_help_coordinator_;
  id<PopupMenuUIUpdating> popupMenuUIUpdating_;
  raw_ptr<feature_engagement::test::MockTracker> tracker_;
};

// Test that blue dot is set on foreground.
TEST_F(PopupMenuHelpCoordinatorTest, ShowBlueDotSetOnForeground) {
  scoped_feature_list_.InitAndEnableFeature(kBlueDotOnToolsMenuButton);
  ON_CALL(
      *tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserOverflowMenuBadgeFeature)))
      .WillByDefault(testing::Return(true));
  OCMExpect([popupMenuUIUpdating_ setOverflowMenuBlueDot:YES]);

  // Move to foreground and check the expectations.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_TRUE([popup_menu_help_coordinator_ hasBlueDotForOverflowMenu]);
  EXPECT_OCMOCK_VERIFY((id)popupMenuUIUpdating_);
}
//

// Test that blue dot is not set on foreground when FET feature is not eligible.
TEST_F(PopupMenuHelpCoordinatorTest, DontShowBlueDotSetOnForeground) {
  scoped_feature_list_.InitAndEnableFeature(kBlueDotOnToolsMenuButton);
  ON_CALL(
      *tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserOverflowMenuBadgeFeature)))
      .WillByDefault(testing::Return(false));
  OCMExpect([popupMenuUIUpdating_ setOverflowMenuBlueDot:NO]);

  // Move to foreground and check the expectations.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE([popup_menu_help_coordinator_ hasBlueDotForOverflowMenu]);
  EXPECT_OCMOCK_VERIFY((id)popupMenuUIUpdating_);
}

}  // namespace
