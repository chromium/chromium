// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/coordinator/content_suggestions_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/content_suggestions/coordinator/content_suggestions_delegate.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/coordinator/magic_stack_ranking_model.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_collection_view.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_magic_stack_mediator.h"
#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_ntp_customization_coordinator.h"
#import "ios/chrome/browser/content_suggestions/tips/model/tips_metrics.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

constexpr char kPriceTrackingPromoCounterPref[] =
    "ephemeral_pref_counter.price_tracking_promo_counter";

std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    ProfileIOS* profile) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}
}  // namespace

@interface ContentSuggestionsCoordinator (Testing) <
    ContentSuggestionsViewControllerAudience,
    MagicStackModuleContainerDelegate,
    TipsNTPCustomizationCoordinatorDelegate>
- (void)logEphemeralCardVisibility:(ContentSuggestionsModuleType)card;
- (void)logTopModuleImpressionForType:(ContentSuggestionsModuleType)moduleType;
@end

// Testing Suite for ContentSuggestionsCoordinator
class ContentSuggestionsCoordinatorTest : public PlatformTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        segmentation_platform::features::kMagicStackNTPThemeTipIos);
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDefaultFactory());
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        ReadingListModelTestingFactoryWithFakeStorage({}));
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
    builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return commerce::MockShoppingService::Build();
            }));
    builder.AddTestingFactory(ios::BookmarkModelFactory::GetInstance(),
                              ios::BookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeSafetyCheckManagerFactory::GetInstance(),
        IOSChromeSafetyCheckManagerFactory::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    scene_state_ = [[SceneState alloc] init];
    [scene_state_ addAgent:[[LayoutGuideSceneAgent alloc] init]];
    browser_ = std::make_unique<TestBrowser>(profile_, scene_state_);
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:OCMProtocolMock(@protocol(SnackbarCommands))
                     forProtocol:@protocol(SnackbarCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:OCMProtocolMock(@protocol(HelpCommands))
                     forProtocol:@protocol(HelpCommands)];

    coordinator_ = [[ContentSuggestionsCoordinator alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()];
    coordinator_.NTPActionsDelegate =
        OCMProtocolMock(@protocol(NewTabPageActionsDelegate));
    [coordinator_ start];
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  ScopedKeyWindow scoped_key_window_;
  ContentSuggestionsCoordinator* coordinator_;
  SceneState* scene_state_;
};

// Tests that calling `-logEphemeralCardVisibility:` updates the correct counter
// without duplicating the top module impression histogram.
TEST_F(ContentSuggestionsCoordinatorTest, TestLogEphemeralCardVisibility) {
  base::HistogramTester histogram_tester;
  [coordinator_ logTopModuleImpressionForType:ContentSuggestionsModuleType::
                                                  kPriceTrackingPromo];
  [coordinator_ logEphemeralCardVisibility:ContentSuggestionsModuleType::
                                               kPriceTrackingPromo];
  EXPECT_EQ(1,
            profile_->GetPrefs()->GetInteger(kPriceTrackingPromoCounterPref));
  histogram_tester.ExpectUniqueSample(
      "IOS.MagicStack.Module.TopImpression",
      ContentSuggestionsModuleType::kPriceTrackingPromo, 1);
}

// Tests that calling `-didSelectTip:` logs the tap metrics and marks the card
// as interacted immediately, without waiting for the module removal completion
// block.
TEST_F(ContentSuggestionsCoordinatorTest,
       TestDidSelectTipLogsMetricsImmediately) {
  base::HistogramTester histogram_tester;
  OCMExpect([coordinator_.NTPActionsDelegate tipsOpened]);
  [coordinator_
      didSelectTip:segmentation_platform::TipIdentifier::kEnhancedSafeBrowsing];
  histogram_tester.ExpectUniqueSample(
      kTipsMagicStackModuleTappedTypeHistogram,
      segmentation_platform::TipIdentifier::kEnhancedSafeBrowsing, 1);
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      "ephemeral_pref_interacted."
      "enhanced_safe_browsing_ephemeral_module_interacted"));
  EXPECT_OCMOCK_VERIFY((id)coordinator_.NTPActionsDelegate);
}

// Tests that calling `-logEphemeralCardVisibility:` defers updating the
// impression counter when `[delegate isNTPBypassed]` returns `YES`, and logs
// it when `-logDeferredEphemeralCardVisibilityIfNeeded` is called after the
// overlay is dismissed without navigating.
TEST_F(ContentSuggestionsCoordinatorTest,
       TestLogEphemeralCardVisibilityDeferredWhenNTPBypassed) {
  __block BOOL is_ntp_bypassed = YES;
  id mockDelegate =
      OCMStrictProtocolMock(@protocol(ContentSuggestionsDelegate));
  OCMStub([mockDelegate isNTPBypassed]).andDo(^(NSInvocation* invocation) {
    [invocation setReturnValue:&is_ntp_bypassed];
  });
  coordinator_.delegate = mockDelegate;

  [coordinator_ logEphemeralCardVisibility:ContentSuggestionsModuleType::
                                               kPriceTrackingPromo];
  EXPECT_EQ(0,
            profile_->GetPrefs()->GetInteger(kPriceTrackingPromoCounterPref));

  // Dismissing the overlay without navigating away from the NTP should log the
  // deferred ephemeral card visibility once.
  is_ntp_bypassed = NO;
  [coordinator_ logDeferredEphemeralCardVisibilityIfNeeded];
  EXPECT_EQ(1,
            profile_->GetPrefs()->GetInteger(kPriceTrackingPromoCounterPref));

  // Subsequent calls should not duplicate the impression.
  [coordinator_ logDeferredEphemeralCardVisibilityIfNeeded];
  EXPECT_EQ(1,
            profile_->GetPrefs()->GetInteger(kPriceTrackingPromoCounterPref));
}

// Tests that a deferred ephemeral card impression is cleared and not logged if
// the user navigates away from the NTP (`webState` changes) before the overlay
// finishes dismissing.
TEST_F(ContentSuggestionsCoordinatorTest,
       TestDeferredEphemeralCardVisibilityClearedOnNavigationAway) {
  web::FakeWebState fake_web_state;
  coordinator_.webState = &fake_web_state;

  __block BOOL is_ntp_bypassed = YES;
  id mockDelegate =
      OCMStrictProtocolMock(@protocol(ContentSuggestionsDelegate));
  OCMStub([mockDelegate isNTPBypassed]).andDo(^(NSInvocation* invocation) {
    [invocation setReturnValue:&is_ntp_bypassed];
  });
  coordinator_.delegate = mockDelegate;

  [coordinator_ logEphemeralCardVisibility:ContentSuggestionsModuleType::
                                               kPriceTrackingPromo];
  EXPECT_EQ(0,
            profile_->GetPrefs()->GetInteger(kPriceTrackingPromoCounterPref));

  // Simulate navigating away from the NTP before the overlay is dismissed.
  coordinator_.webState = nullptr;
  is_ntp_bypassed = NO;
  [coordinator_ logDeferredEphemeralCardVisibilityIfNeeded];
  EXPECT_EQ(0,
            profile_->GetPrefs()->GetInteger(kPriceTrackingPromoCounterPref));
}

// Tests that calling `-didSelectNTPThemePhotoPicker` and
// `-didSelectNTPThemeOption` logs `kTipsMagicStackModuleTappedTypeHistogram`,
// calls `-tipsOpened`, and marks the NTP Theme ephemeral module as interacted.
TEST_F(ContentSuggestionsCoordinatorTest,
       TestNTPThemeActionableTipInteractionsLogMetrics) {
  base::HistogramTester histogram_tester;
  id mockDelegate =
      OCMStrictProtocolMock(@protocol(ContentSuggestionsDelegate));
  coordinator_.delegate = mockDelegate;

  OCMExpect([mockDelegate openNTPBackgroundPhotoPicker]);
  OCMExpect([coordinator_.NTPActionsDelegate tipsOpened]);
  [coordinator_ didSelectNTPThemePhotoPicker];
  histogram_tester.ExpectUniqueSample(
      kTipsMagicStackModuleTappedTypeHistogram,
      segmentation_platform::TipIdentifier::kNTPTheme, 1);
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      "ephemeral_pref_interacted.ntp_theme_ephemeral_module_interacted"));

  OCMExpect([coordinator_.NTPActionsDelegate tipsOpened]);
  [coordinator_ didSelectNTPThemeOption];
  histogram_tester.ExpectUniqueSample(
      kTipsMagicStackModuleTappedTypeHistogram,
      segmentation_platform::TipIdentifier::kNTPTheme, 2);
  EXPECT_OCMOCK_VERIFY(mockDelegate);
  EXPECT_OCMOCK_VERIFY((id)coordinator_.NTPActionsDelegate);
}

// Tests that tapping "See More" on `kTipsNTPThemeActionable` logs Magic Stack
// engagement for `kTipsNTPThemeActionable` and `kNTPTheme` in
// `kTipsMagicStackModuleTappedTypeHistogram`.
TEST_F(ContentSuggestionsCoordinatorTest,
       TestSeeMoreForNTPThemeActionableLogsEngagement) {
  base::HistogramTester histogram_tester;
  OCMExpect([coordinator_.NTPActionsDelegate tipsOpened]);
  [coordinator_ seeMoreWasTappedForModuleType:ContentSuggestionsModuleType::
                                                  kTipsNTPThemeActionable];
  histogram_tester.ExpectUniqueSample(
      "IOS.MagicStack.Module.Click",
      ContentSuggestionsModuleType::kTipsNTPThemeActionable, 1);
  histogram_tester.ExpectUniqueSample(
      kTipsMagicStackModuleTappedTypeHistogram,
      segmentation_platform::TipIdentifier::kNTPTheme, 1);
  EXPECT_OCMOCK_VERIFY((id)coordinator_.NTPActionsDelegate);
}

// Tests that `-tipsNTPCustomizationCoordinator:didFinishWithAccept:` logs
// `kTipsNTPCustomizationPromoActionHistogram` for both accept and cancel.
TEST_F(ContentSuggestionsCoordinatorTest,
       TestTipsNTPCustomizationCoordinatorLogsPromoAction) {
  base::HistogramTester histogram_tester;
  id mockDelegate =
      OCMStrictProtocolMock(@protocol(ContentSuggestionsDelegate));
  coordinator_.delegate = mockDelegate;

  OCMExpect([mockDelegate applyFocusedNTPWithUndoSnackbar]);
  [coordinator_ tipsNTPCustomizationCoordinator:nil didFinishWithAccept:YES];
  histogram_tester.ExpectBucketCount(
      kTipsNTPCustomizationPromoActionHistogram,
      IOSTipsNTPCustomizationPromoAction::kAccepted, 1);

  [coordinator_ tipsNTPCustomizationCoordinator:nil didFinishWithAccept:NO];
  histogram_tester.ExpectBucketCount(
      kTipsNTPCustomizationPromoActionHistogram,
      IOSTipsNTPCustomizationPromoAction::kCancelled, 1);
  EXPECT_OCMOCK_VERIFY(mockDelegate);
}
