// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_magic_stack_mediator.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/test/test_bookmark_client.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_magic_stack_mediator_delegate.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/ntp_theme_actionable_tip_config.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_audience.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_config.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using segmentation_platform::TipIdentifier;

// Tests the `TipsMagicStackMediator`.
class TipsMagicStackMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    profile_pref_service_.registry()->RegisterBooleanPref(
        ntp_tiles::prefs::kTipsHomeModuleEnabled, true);

    HomeBackgroundCustomizationService::RegisterProfilePrefs(
        profile_pref_service_.registry());

    // Create a `TipsMagicStackMediator` with an initial unknown
    // `TipIdentifier`.
    mediator_ = [[TipsMagicStackMediator alloc]
                    initWithIdentifier:TipIdentifier::kUnknown
                    profilePrefService:&profile_pref_service_
                       shoppingService:shopping_service_.get()
                         bookmarkModel:bookmark_model_.get()
                          imageFetcher:
                              std::make_unique<image_fetcher::ImageDataFetcher>(
                                  profile_->GetSharedURLLoaderFactory())
        backgroundCustomizationService:
            HomeBackgroundCustomizationServiceFactory::GetForProfile(
                profile_.get())];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TipsMagicStackMediator* mediator_;
  std::unique_ptr<TestProfileIOS> profile_;
  sync_preferences::TestingPrefServiceSyncable profile_pref_service_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

// Tests that the mediator's initial state is configured correctly for an
// unknown tip.
TEST_F(TipsMagicStackMediatorTest, HasCorrectInitialStateForUnknownTip) {
  EXPECT_EQ(TipIdentifier::kUnknown, mediator_.config.identifier);
}

// Tests that the mediator reconfigures its state to reflect a new tip.
TEST_F(TipsMagicStackMediatorTest, ReconfiguresStateForNewTip) {
  [mediator_ reconfigureWithTipIdentifier:TipIdentifier::kLensShop];

  EXPECT_EQ(TipIdentifier::kLensShop, mediator_.config.identifier);
}

// Tests that the mediator calls `-removeTipsModule` on its delegate when
// the card is disabled.
TEST_F(TipsMagicStackMediatorTest, CallsRemoveModuleOnDelegate) {
  id delegate =
      OCMStrictProtocolMock(@protocol(TipsMagicStackMediatorDelegate));
  mediator_.delegate = delegate;

  // Set up the expectation for the delegate method call, using the
  // completionBlock.
  OCMExpect([delegate removeTipsModuleWithCompletion:[OCMArg any]]);

  profile_pref_service_.SetBoolean(ntp_tiles::prefs::kTipsHomeModuleEnabled,
                                   false);

  // Verify that the delegate method was called.
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the mediator logs Magic Stack engagement and forwards
// `-didSelectTip:` to its `presentationAudience`.
TEST_F(TipsMagicStackMediatorTest, ForwardsDidSelectTipAndLogsEngagement) {
  id delegate =
      OCMStrictProtocolMock(@protocol(TipsMagicStackMediatorDelegate));
  mediator_.delegate = delegate;
  id audience = OCMStrictProtocolMock(
      @protocol(ContentSuggestionsViewControllerAudience));
  mediator_.presentationAudience = audience;

  OCMExpect([delegate
      logMagicStackEngagementForType:ContentSuggestionsModuleType::kTips]);
  OCMExpect([audience didSelectTip:TipIdentifier::kSavePasswords]);
  [(id<TipsModuleAudience>)mediator_
      didSelectTip:TipIdentifier::kSavePasswords];
  EXPECT_OCMOCK_VERIFY(delegate);
  EXPECT_OCMOCK_VERIFY(audience);
}

// Tests that the mediator logs Magic Stack engagement and forwards
// `-didSelectNTPThemePhotoPicker` to its `presentationAudience`.
TEST_F(TipsMagicStackMediatorTest, ForwardsDidSelectNTPThemePhotoPicker) {
  id delegate =
      OCMStrictProtocolMock(@protocol(TipsMagicStackMediatorDelegate));
  mediator_.delegate = delegate;
  id audience = OCMStrictProtocolMock(
      @protocol(ContentSuggestionsViewControllerAudience));
  mediator_.presentationAudience = audience;
  OCMExpect([delegate
      logMagicStackEngagementForType:ContentSuggestionsModuleType::kTips]);
  OCMExpect([audience didSelectNTPThemePhotoPicker]);
  [(id<TipsModuleAudience>)mediator_ didSelectNTPThemePhotoPicker];
  EXPECT_OCMOCK_VERIFY(delegate);
  EXPECT_OCMOCK_VERIFY(audience);
}

// Tests that the mediator creates an `NTPThemeActionableTipConfig` when
// reconfigured with `kNTPTheme` and the actionable variation is enabled, and
// resets it when reconfigured with an informational tip.
TEST_F(TipsMagicStackMediatorTest,
       ReconfiguresActionableConfigForNTPThemeWhenActionableEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      segmentation_platform::features::kMagicStackNTPThemeTipIos,
      {{segmentation_platform::features::
            kMagicStackNTPThemeTipIosActionableParam,
        "true"}});
  [mediator_ reconfigureWithTipIdentifier:TipIdentifier::kNTPTheme];
  EXPECT_EQ(TipIdentifier::kNTPTheme, mediator_.config.identifier);
  EXPECT_EQ(ContentSuggestionsModuleType::kTipsNTPThemeActionable,
            mediator_.activeConfig.type);

  [mediator_ reconfigureWithTipIdentifier:TipIdentifier::kLensShop];
  EXPECT_EQ(mediator_.config, mediator_.activeConfig);
}

// Tests that selecting an NTP theme swatch updates `selectedIndex`, applies
// the theme to `HomeBackgroundCustomizationService`, logs Magic Stack
// engagement, and notifies `presentationAudience`.
TEST_F(TipsMagicStackMediatorTest, AppliesSelectedNTPThemeAtIndex) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      segmentation_platform::features::kMagicStackNTPThemeTipIos,
      {{segmentation_platform::features::
            kMagicStackNTPThemeTipIosActionableParam,
        "true"}});
  [mediator_ reconfigureWithTipIdentifier:TipIdentifier::kNTPTheme];
  ASSERT_EQ(ContentSuggestionsModuleType::kTipsNTPThemeActionable,
            mediator_.activeConfig.type);
  NTPThemeActionableTipConfig* actionableConfig =
      static_cast<NTPThemeActionableTipConfig*>(mediator_.activeConfig);
  HomeBackgroundCustomizationService* service =
      HomeBackgroundCustomizationServiceFactory::GetForProfile(profile_.get());
  id delegate =
      OCMStrictProtocolMock(@protocol(TipsMagicStackMediatorDelegate));
  mediator_.delegate = delegate;
  id audience = OCMStrictProtocolMock(
      @protocol(ContentSuggestionsViewControllerAudience));
  mediator_.presentationAudience = audience;

  OCMExpect(
      [delegate logMagicStackEngagementForType:ContentSuggestionsModuleType::
                                                   kTipsNTPThemeActionable]);
  OCMExpect([audience didSelectNTPThemeOption]);
  [(id<TipsModuleAudience>)mediator_ didSelectNTPThemeAtIndex:1];
  EXPECT_EQ(1, actionableConfig.selectedIndex);
  EXPECT_TRUE(service->GetCurrentColorTheme().has_value());
  EXPECT_OCMOCK_VERIFY(delegate);
  EXPECT_OCMOCK_VERIFY(audience);

  OCMExpect(
      [delegate logMagicStackEngagementForType:ContentSuggestionsModuleType::
                                                   kTipsNTPThemeActionable]);
  OCMExpect([audience didSelectNTPThemeOption]);
  [(id<TipsModuleAudience>)mediator_ didSelectNTPThemeAtIndex:0];
  EXPECT_EQ(0, actionableConfig.selectedIndex);
  EXPECT_FALSE(service->GetCurrentColorTheme().has_value());
  EXPECT_OCMOCK_VERIFY(delegate);
  EXPECT_OCMOCK_VERIFY(audience);
}

// Tests that the mediator logs Magic Stack engagement and forwards
// `-didToggleTipsNotificationsSwitch:` to its `presentationAudience`.
TEST_F(TipsMagicStackMediatorTest, ForwardsDidToggleTipsNotificationsSwitch) {
  id delegate =
      OCMStrictProtocolMock(@protocol(TipsMagicStackMediatorDelegate));
  mediator_.delegate = delegate;
  id audience = OCMStrictProtocolMock(
      @protocol(ContentSuggestionsViewControllerAudience));
  mediator_.presentationAudience = audience;

  OCMExpect([delegate
      logMagicStackEngagementForType:ContentSuggestionsModuleType::kTips]);
  OCMExpect([audience didToggleTipsNotificationsSwitch:YES]);
  [(id<TipsModuleAudience>)mediator_ didToggleTipsNotificationsSwitch:YES];
  EXPECT_OCMOCK_VERIFY(delegate);
  EXPECT_OCMOCK_VERIFY(audience);
}
