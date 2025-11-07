// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_consumer.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Fake consumer for the main page of the Customization menu.
@interface FakeHomeCustomizationMainConsumer
    : NSObject <HomeCustomizationMainConsumer>

// The toggles, populated by `populateToggles:` to faciliate testing.
@property(nonatomic, assign) std::map<CustomizationToggleType, BOOL> toggleMap;

@end

@implementation FakeHomeCustomizationMainConsumer

#pragma mark - HomeCustomizationMainConsumer

- (void)populateToggles:(std::map<CustomizationToggleType, BOOL>)toggleMap {
  _toggleMap = toggleMap;
}

@end

// Tests for the Home Customization mediator.
class HomeCustomizationMediatorUnitTest : public PlatformTest {
 public:
  void SetUp() override {
    profile_ = TestProfileIOS::Builder().Build();
    Browser* browser = new TestBrowser(profile_.get());
    DiscoverFeedVisibilityBrowserAgent::CreateForBrowser(browser);
    pref_service_ = profile_->GetPrefs();
    discover_feed_visibility_browser_agent_ =
        DiscoverFeedVisibilityBrowserAgent::FromBrowser(browser);

    mediator_ =
        [[HomeCustomizationMediator alloc] initWithPrefService:pref_service_
                            discoverFeedVisibilityBrowserAgent:
                                discover_feed_visibility_browser_agent_
                                               shoppingService:nil];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  HomeCustomizationMediator* mediator_;
  raw_ptr<DiscoverFeedVisibilityBrowserAgent, DanglingUntriaged>
      discover_feed_visibility_browser_agent_;
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that the mediator populates the main page data for its consumer based
// on the profile's prefs.
TEST_F(HomeCustomizationMediatorUnitTest, TestMainPageData) {
  FakeHomeCustomizationMainConsumer* fake_consumer =
      [[FakeHomeCustomizationMainConsumer alloc] init];
  mediator_.mainPageConsumer = fake_consumer;

  // Set the values.
  pref_service_->SetBoolean(ntp_tiles::prefs::kMostVisitedHomeModuleEnabled,
                            NO);
  pref_service_->SetBoolean(ntp_tiles::prefs::kMagicStackHomeModuleEnabled,
                            YES);
  discover_feed_visibility_browser_agent_->SetEnabled(NO);

  [mediator_ configureMainPageData];

  // Check that the toggles properly reflect the updated values.
  std::map<CustomizationToggleType, BOOL> toggle_map = fake_consumer.toggleMap;
  EXPECT_EQ(toggle_map.at(CustomizationToggleType::kMostVisited), NO);
  EXPECT_EQ(toggle_map.at(CustomizationToggleType::kMagicStack), YES);
  EXPECT_EQ(toggle_map.at(CustomizationToggleType::kDiscover), NO);
}
