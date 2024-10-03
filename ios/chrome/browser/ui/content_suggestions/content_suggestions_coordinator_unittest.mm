// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_collection_view.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/magic_stack_parcel_list_half_sheet_table_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    web::BrowserState* browser_state) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}
}  // namespace

@interface ContentSuggestionsCoordinator (Testing) <
    MagicStackModuleContainerDelegate>
- (void)logEphemeralCardVisibility:(ContentSuggestionsModuleType)card;
@end

// Testing Suite for ContentSuggestionsCoordinator
class ContentSuggestionsCoordinatorTest : public PlatformTest {
 public:
  void SetUp() override {
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
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::vector<scoped_refptr<ReadingListEntry>>()));
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
    builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return commerce::MockShoppingService::Build();
            }));

    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_, std::make_unique<FakeAuthenticationServiceDelegate>());

    browser_ = std::make_unique<TestBrowser>(profile_);
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());

    coordinator_ = [[ContentSuggestionsCoordinator alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()];
    coordinator_.NTPActionsDelegate =
        OCMProtocolMock(@protocol(NewTabPageActionsDelegate));
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  ScopedKeyWindow scoped_key_window_;
  ContentSuggestionsCoordinator* coordinator_;
};

// Tests that calling -logEphemeralCardVisibility: updates the correct counter.
TEST_F(ContentSuggestionsCoordinatorTest, TestLogEphemeralCardVisibility) {
  [coordinator_ logEphemeralCardVisibility:ContentSuggestionsModuleType::
                                               kPriceTrackingPromo];
  EXPECT_EQ(1, profile_->GetTestingPrefService()->GetInteger(
                   "ephemeral_pref_counter.price_tracking_promo_counter"));
}

// Tests that calling -seeMoreWasTappedForModuleType:kParcelTracking shows the
// Parcel Tracking modal.
TEST_F(ContentSuggestionsCoordinatorTest,
       TestSeeMoreWasTappedForParcelTracking) {
  [coordinator_ start];
  [scoped_key_window_.Get()
      setRootViewController:coordinator_.magicStackCollectionView];
  [coordinator_ seeMoreWasTappedForModuleType:ContentSuggestionsModuleType::
                                                  kParcelTracking];

  UINavigationController* viewController =
      base::apple::ObjCCastStrict<UINavigationController>(
          coordinator_.magicStackCollectionView.presentedViewController);
  ASSERT_EQ([MagicStackParcelListHalfSheetTableViewController class],
            [viewController.topViewController class]);

  [coordinator_ stop];
}
