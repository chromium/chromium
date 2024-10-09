// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model.h"

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_timeouts.h"
#import "base/threading/thread_restrictions.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/ntp_tiles/icon_cacher.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
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
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator+testing.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_magic_stack_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_mediator.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using set_up_list_prefs::SetUpListItemState;
using startup_metric_utils::FirstRunSentinelCreationResult;

namespace {
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    web::BrowserState* browser_state) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}
}  // namespace

// Expose -addConsumer: to validate consumer calls.
@interface SetUpListMediator (Testing)
- (void)addConsumer:(id<SetUpListConsumer>)consumer;
@end

// Fake subclass of SetUpListMediator to easily allow for Set Up List to be
// shown.
@interface FakeSetUpListMediator : SetUpListMediator

// Allows enabling or disabling the SetUpList.
@property(nonatomic, assign) BOOL shouldShowSetUpList;

@end

@implementation FakeSetUpListMediator
@end

// Fake subclass of ParcelTrackingMediator to override item config construction.
@interface FakeParcelTrackingMediator : ParcelTrackingMediator
@end

@implementation FakeParcelTrackingMediator {
  ParcelTrackingItem* _item;
}
- (ParcelTrackingItem*)parcelTrackingItemToShow {
  if (!_item) {
    _item = [[ParcelTrackingItem alloc] init];
  }
  return _item;
}
@end

// Fake subclass of TabResumptionMediator to override item config construction.
@interface FakeTabResumptionMediator : TabResumptionMediator
@end

@implementation FakeTabResumptionMediator {
  TabResumptionItem* _item;
}

- (TabResumptionItem*)itemConfig {
  if (!_item) {
    _item = [[TabResumptionItem alloc] initWithItemType:kMostRecentTab];
    _item.tabURL = GURL("http://test.com");
  }
  return _item;
}

- (void)fetchLastTabResumptionItem {
}

@end

// Fake subclass of MostVisitedTilesMediator to override item config
// construction.
@interface FakeMostVisitedTilesMediator : MostVisitedTilesMediator
@end

@implementation FakeMostVisitedTilesMediator {
  MostVisitedTilesConfig* _config;
}

- (MostVisitedTilesConfig*)mostVisitedConfig {
  if (!_config) {
    _config = [[MostVisitedTilesConfig alloc] init];
    _config.mostVisitedItems =
        @[ [[ContentSuggestionsMostVisitedItem alloc] init] ];
  }
  return _config;
}

@end

// Fake MagicStackRankingModelDelegate receiver to validate results sent by
// MagicStackRankingModel.
@interface FakeMagicStackRankingModelDelegate
    : NSObject <MagicStackRankingModelDelegate>
@property(strong, nonatomic) NSArray<MagicStackModule*>* rank;
@property(strong, nonatomic) MagicStackModule* lastInsertedItem;
@property(nonatomic, assign) NSUInteger lastInsertionIndex;
@property(strong, nonatomic) MagicStackModule* lastReplacedItem;
@property(strong, nonatomic) MagicStackModule* lastReplacingItem;
@end

@implementation FakeMagicStackRankingModelDelegate

- (void)magicStackRankingModel:(MagicStackRankingModel*)model
      didGetLatestRankingOrder:(NSArray<MagicStackModule*>*)rank {
  _rank = rank;
}
- (void)magicStackRankingModel:(MagicStackRankingModel*)model
                 didInsertItem:(MagicStackModule*)item
                       atIndex:(NSUInteger)index {
  _lastInsertedItem = item;
  _lastInsertionIndex = index;
}
- (void)magicStackRankingModel:(MagicStackRankingModel*)model
                didReplaceItem:(MagicStackModule*)oldItem
                      withItem:(MagicStackModule*)item {
  _lastReplacedItem = oldItem;
  _lastReplacingItem = item;
}

- (void)magicStackRankingModel:(MagicStackRankingModel*)model
                 didRemoveItem:(MagicStackModule*)item {
}

- (void)magicStackRankingModel:(MagicStackRankingModel*)model
            didReconfigureItem:(MagicStackModule*)item {
}

@end

// Expose -hasReceivedMagicStackResponse for waiting for ranking to return.
@interface MagicStackRankingModel (Testing) <
    MostVisitedTilesMediatorDelegate,
    ParcelTrackingMediatorDelegate,
    SafetyCheckMagicStackMediatorDelegate,
    TabResumptionHelperDelegate>
@property(nonatomic, assign, readonly) BOOL hasReceivedMagicStackResponse;
@property(nonatomic, assign, readonly) BOOL hasReceivedEphemericalCardResponse;
@end

// Testing Suite for MagicStackRankingModel
class MagicStackRankingModelTest : public PlatformTest {
 public:
  void SetUp() override {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        segmentation_platform::kEphemeralModuleBackendRankerTestOverride,
        "price_tracking_notification_promo");
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kMagicStack, {{kMagicStackMostVisitedModuleParam, "true"}}}}, {});

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

    browser_ = std::make_unique<TestBrowser>(GetProfile());

    // Necessary set up for kIOSSetUpList.
    GetLocalState()->ClearPref(set_up_list_prefs::kDisabled);
    ClearDefaultBrowserPromoData();
    WriteFirstRunSentinel();

    // Necessary set up for parcel tracking.
    scoped_variations_service_ =
        std::make_unique<IOSChromeScopedTestingVariationsService>();
    scoped_variations_service_->Get()->OverrideStoredPermanentCountry("us");

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        GetProfile(), std::make_unique<FakeAuthenticationServiceDelegate>());
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForProfile(GetProfile());
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForProfile(GetProfile());
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(GetProfile());

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());

    ReadingListModel* readingListModel =
        ReadingListModelFactory::GetForProfile(GetProfile());
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(GetProfile());
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(GetProfile());
    _shortcutsMediator = [[ShortcutsMediator alloc]
        initWithReadingListModel:readingListModel
        featureEngagementTracker:(feature_engagement::Tracker*)tracker
                     authService:authentication_service];
    _setUpListMediator = [[FakeSetUpListMediator alloc]
                   initWithPrefService:GetProfile()->GetPrefs()
                           syncService:syncService
                       identityManager:identityManager
                 authenticationService:authenticationService
                            sceneState:scene_state_
                 isDefaultSearchEngine:NO
                   segmentationService:nullptr
        deviceSwitcherResultDispatcher:nullptr];
    _setUpListMediator.shouldShowSetUpList = YES;
    _parcelTrackingMediator = [[FakeParcelTrackingMediator alloc]
        initWithShoppingService:commerce::ShoppingServiceFactory::GetForProfile(
                                    GetProfile())
         URLLoadingBrowserAgent:url_loader_
                    prefService:GetLocalState()];
    _tabResumptionMediator = [[FakeTabResumptionMediator alloc]
        initWithLocalState:GetLocalState()
               prefService:GetProfile()->GetPrefs()
           identityManager:identityManager
                   browser:browser_.get()];
    favicon::LargeIconService* large_icon_service =
        IOSChromeLargeIconServiceFactory::GetForProfile(GetProfile());
    LargeIconCache* cache =
        IOSChromeLargeIconCacheFactory::GetForProfile(GetProfile());
    std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites =
        std::make_unique<ntp_tiles::MostVisitedSites>(
            &pref_service_, /*identity_manager*/ nullptr,
            /*supervised_user_service*/ nullptr, /*top_sites*/ nullptr,
            /*popular_sites*/ nullptr,
            /*custom_links*/ nullptr, /*icon_cacher*/ nullptr, true);
    _mostVisitedTilesMediator = [[FakeMostVisitedTilesMediator alloc]
        initWithMostVisitedSite:std::move(most_visited_sites)
                    prefService:GetProfile()->GetPrefs()
               largeIconService:large_icon_service
                 largeIconCache:cache
         URLLoadingBrowserAgent:url_loader_];

    id mockAppState = OCMClassMock([AppState class]);

    _safetyCheckMediator = [[SafetyCheckMagicStackMediator alloc]
        initWithSafetyCheckManager:IOSChromeSafetyCheckManagerFactory::
                                       GetForProfile(GetProfile())
                        localState:GetLocalState()
                         userState:GetProfile()->GetPrefs()
                          appState:mockAppState];

    _priceTrackingPromoMediator = [[PriceTrackingPromoMediator alloc]
        initWithShoppingService:commerce::ShoppingServiceFactory::GetForProfile(
                                    GetProfile())
                  bookmarkModel:nil
                   imageFetcher:nil
                    prefService:GetProfile()->GetPrefs()
                     localState:GetLocalState()
        pushNotificationService:nil
          authenticationService:nil];

    PriceTrackingPromoItem* item = [[PriceTrackingPromoItem alloc] init];
    [_priceTrackingPromoMediator setPriceTrackingPromoItemForTesting:item];

    _magicStackRankingModel = [[MagicStackRankingModel alloc]
        initWithSegmentationService:segmentation_platform::
                                        SegmentationPlatformServiceFactory::
                                            GetForProfile(GetProfile())
                    shoppingService:commerce::ShoppingServiceFactory::
                                        GetForProfile(GetProfile())
                        authService:authenticationService
                        prefService:GetProfile()->GetPrefs()
                         localState:GetLocalState()
                    moduleMediators:@[
                      _shortcutsMediator,
                      _setUpListMediator,
                      _parcelTrackingMediator,
                      _tabResumptionMediator,
                      _mostVisitedTilesMediator,
                      _safetyCheckMediator,
                      _priceTrackingPromoMediator,
                    ]
                        tipsManager:TipsManagerIOSFactory::GetForProfile(
                                        browser_->GetProfile())];

    metrics_recorder_ = [[ContentSuggestionsMetricsRecorder alloc]
        initWithLocalState:GetLocalState()];
    _magicStackRankingModel.contentSuggestionsMetricsRecorder =
        metrics_recorder_;
    _setUpListMediator.contentSuggestionsMetricsRecorder = metrics_recorder_;

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  ProfileIOS* GetProfile() { return profile_.get(); }

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  ~MagicStackRankingModelTest() override {
    [_setUpListMediator disconnect];
    [_tabResumptionMediator disconnect];
    [_parcelTrackingMediator disconnect];
    [_shortcutsMediator disconnect];
    [_mostVisitedTilesMediator disconnect];
    [_safetyCheckMediator disconnect];
    [_priceTrackingPromoMediator disconnect];
  }

 protected:
  // Clears and re-writes the FirstRun sentinel file, in order to allow Set Up
  // List to display.
  void WriteFirstRunSentinel() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    FirstRun::RemoveSentinel();
    base::File::Error file_error = base::File::FILE_OK;
    FirstRunSentinelCreationResult sentinel_created =
        FirstRun::CreateSentinel(&file_error);
    ASSERT_EQ(sentinel_created, FirstRunSentinelCreationResult::kSuccess)
        << "Error creating FirstRun sentinel: "
        << base::File::ErrorToString(file_error);
    FirstRun::LoadSentinelInfo();
    FirstRun::ClearStateForTesting();
    EXPECT_FALSE(set_up_list_prefs::IsSetUpListDisabled(GetLocalState()));
    EXPECT_FALSE(FirstRun::IsChromeFirstRun());
    EXPECT_TRUE(set_up_list_utils::IsSetUpListActive(GetLocalState()));
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  FakeSceneState* scene_state_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  std::unique_ptr<IOSChromeScopedTestingVariationsService>
      scoped_variations_service_;
  FakeSetUpListMediator* _setUpListMediator;
  FakeParcelTrackingMediator* _parcelTrackingMediator;
  FakeTabResumptionMediator* _tabResumptionMediator;
  ShortcutsMediator* _shortcutsMediator;
  SafetyCheckMagicStackMediator* _safetyCheckMediator;
  MostVisitedTilesMediator* _mostVisitedTilesMediator;
  PriceTrackingPromoMediator* _priceTrackingPromoMediator;
  MagicStackRankingModel* _magicStackRankingModel;
  id setUpListConsumer_;
  ContentSuggestionsMetricsRecorder* metrics_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(MagicStackRankingModelTest, TestSetUpListConsumerCall) {
  setUpListConsumer_ = OCMStrictProtocolMock(@protocol(SetUpListConsumer));
  [_setUpListMediator addConsumer:setUpListConsumer_];
  [_magicStackRankingModel fetchLatestMagicStackRanking];

  OCMExpect([setUpListConsumer_ setUpListItemDidComplete:[OCMArg any]
                                       allItemsCompleted:NO
                                              completion:[OCMArg any]]);
  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kSignInSync);
  OCMExpect([setUpListConsumer_ setUpListItemDidComplete:[OCMArg any]
                                       allItemsCompleted:NO
                                              completion:[OCMArg any]]);
  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kDefaultBrowser);
  OCMExpect([setUpListConsumer_ setUpListItemDidComplete:[OCMArg any]
                                       allItemsCompleted:YES
                                              completion:[OCMArg any]]);
  set_up_list_prefs::MarkItemComplete(GetLocalState(),
                                      SetUpListItemType::kAutofill);
  EXPECT_OCMOCK_VERIFY(setUpListConsumer_);
}

// Tests that SetUpList metrics are recorded when it is in the MagicStack.
TEST_F(MagicStackRankingModelTest, TestMetricsWithSetUpList) {
  [_magicStackRankingModel fetchLatestMagicStackRanking];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), true, ^bool() {
        base::RunLoop().RunUntilIdle();
        return _magicStackRankingModel.hasReceivedMagicStackResponse;
      }));
  histogram_tester_->ExpectUniqueSample("IOS.SetUpList.Displayed", true, 1);
  histogram_tester_->ExpectTotalCount("IOS.SetUpList.ItemDisplayed", 2);
  histogram_tester_->ExpectBucketCount("IOS.SetUpList.ItemDisplayed",
                                       SetUpListItemType::kDefaultBrowser, 1);
  histogram_tester_->ExpectBucketCount("IOS.SetUpList.ItemDisplayed",
                                       SetUpListItemType::kAutofill, 1);
}

// Tests that SetUpList metrics are not recorded when it is not in the
// MagicStack.
TEST_F(MagicStackRankingModelTest, TestMetricsWithoutSetUpList) {
  _setUpListMediator.shouldShowSetUpList = NO;
  [_magicStackRankingModel fetchLatestMagicStackRanking];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), true, ^bool() {
        base::RunLoop().RunUntilIdle();
        return _magicStackRankingModel.hasReceivedMagicStackResponse;
      }));
  histogram_tester_->ExpectTotalCount("IOS.SetUpList.Displayed", 0);
  histogram_tester_->ExpectTotalCount("IOS.SetUpList.ItemDisplayed", 0);
}

// Tests that when the user changes the setting to disable signin, the
// SetUpList signin item is marked complete.
TEST_F(MagicStackRankingModelTest, TestOnServiceStatusChanged) {
  // Verify the initial state.
  SetUpListItemState item_state = set_up_list_prefs::GetItemState(
      GetLocalState(), SetUpListItemType::kSignInSync);
  EXPECT_EQ(item_state, SetUpListItemState::kNotComplete);

  // Simulate the user disabling signin.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  // Verify that the signin item is complete.
  item_state = set_up_list_prefs::GetItemState(GetLocalState(),
                                               SetUpListItemType::kSignInSync);
  EXPECT_EQ(item_state, SetUpListItemState::kCompleteInList);
}

// Tests that logging for IOS.MagicStack.Module.Click.[ModuleName] works
// correctly.
TEST_F(MagicStackRankingModelTest, TestModuleClickIndexMetric) {
  [_magicStackRankingModel fetchLatestMagicStackRanking];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), true, ^bool() {
        base::RunLoop().RunUntilIdle();
        return _magicStackRankingModel.hasReceivedMagicStackResponse;
      }));

  [_magicStackRankingModel logMagicStackEngagementForType:
                               ContentSuggestionsModuleType::kSetUpListSync];
  histogram_tester_->ExpectUniqueSample("IOS.MagicStack.Module.Click.SetUpList",
                                        0, 1);

  [_magicStackRankingModel logMagicStackEngagementForType:
                               ContentSuggestionsModuleType::kMostVisited];
  histogram_tester_->ExpectUniqueSample(
      "IOS.MagicStack.Module.Click.MostVisited", 1, 1);
}

// Test that the ranking model passed an expected list of module configs in
// -didGetLatestRankingOrder:
TEST_F(MagicStackRankingModelTest, TestModelDidGetLatestRankingOrder) {
  FakeMagicStackRankingModelDelegate* delegate_ =
      [[FakeMagicStackRankingModelDelegate alloc] init];
  _magicStackRankingModel.delegate = delegate_;
  [_magicStackRankingModel fetchLatestMagicStackRanking];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), true, ^bool() {
        base::RunLoop().RunUntilIdle();
        return [delegate_.rank count] > 0;
      }));
  NSArray* expectedModuleRank = @[ @(5), @(0), @(1), @(10), @(11) ];
  EXPECT_EQ([delegate_.rank count], [expectedModuleRank count]);
  for (NSUInteger i = 0; i < [expectedModuleRank count]; i++) {
    MagicStackModule* config = delegate_.rank[i];
    EXPECT_EQ(@(int(config.type)), expectedModuleRank[i])
        << "For Magic Stack order index " << i;
  }
}

// Tests that the ranking model sends insertion signals to its delgate in
// response to feature delegate signals.
TEST_F(MagicStackRankingModelTest, TestFeatureInsertCalls) {
  FakeMagicStackRankingModelDelegate* delegate_ =
      [[FakeMagicStackRankingModelDelegate alloc] init];
  _magicStackRankingModel.delegate = delegate_;
  [_magicStackRankingModel fetchLatestMagicStackRanking];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), true, ^bool() {
        base::RunLoop().RunUntilIdle();
        return [delegate_.rank count] > 0;
      }));

  [_magicStackRankingModel newParcelsAvailable];
  EXPECT_EQ(delegate_.lastInsertionIndex, 4u);
  EXPECT_EQ(delegate_.lastInsertedItem,
            _parcelTrackingMediator.parcelTrackingItemToShow);

  [_magicStackRankingModel tabResumptionHelperDidReceiveItem];
  EXPECT_EQ(delegate_.lastInsertionIndex, 3u);
  EXPECT_EQ(delegate_.lastInsertedItem, _tabResumptionMediator.itemConfig);
}

// Test the TestMostVisitedTilesMediatorDelegate API implementations in
// MagicStackRankingModel.
TEST_F(MagicStackRankingModelTest, TestMostVisitedTilesMediatorDelegate) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kMagicStack, {{kMagicStackMostVisitedModuleParam, "true"}}}}, {});

  // Assert that delegate API isn't called if rank has not been received yet.
  id mockDelegate =
      OCMStrictProtocolMock(@protocol(MagicStackRankingModelDelegate));
  _magicStackRankingModel.delegate = mockDelegate;
  [_magicStackRankingModel didReceiveInitialMostVistedTiles];
  [_magicStackRankingModel removeMostVisitedTilesModule];
  EXPECT_OCMOCK_VERIFY(mockDelegate);

  FakeMagicStackRankingModelDelegate* fakeDelegate =
      [[FakeMagicStackRankingModelDelegate alloc] init];
  _magicStackRankingModel.delegate = fakeDelegate;
  [_magicStackRankingModel fetchLatestMagicStackRanking];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), true, ^bool() {
        base::RunLoop().RunUntilIdle();
        return [fakeDelegate.rank count] > 0;
      }));

  _magicStackRankingModel.delegate = mockDelegate;
  OCMExpect([mockDelegate magicStackRankingModel:[OCMArg any]
                                   didInsertItem:[OCMArg any]
                                         atIndex:1]);
  [_magicStackRankingModel didReceiveInitialMostVistedTiles];
  OCMExpect([mockDelegate magicStackRankingModel:[OCMArg any]
                                   didRemoveItem:[OCMArg any]]);
  [_magicStackRankingModel removeMostVisitedTilesModule];
  EXPECT_OCMOCK_VERIFY(mockDelegate);
}

// Verifies that the ranking model correctly emits removal signals to its
// delegate in response to feature delegate signals.
TEST_F(MagicStackRankingModelTest,
       TestSafetyCheckMediatorDelegateCallsRemoval) {
  // Assert that delegate API isn't called if rank has not been received yet.
  id mockDelegate =
      OCMStrictProtocolMock(@protocol(MagicStackRankingModelDelegate));
  _magicStackRankingModel.delegate = mockDelegate;
  [_magicStackRankingModel removeSafetyCheckModule];
  EXPECT_OCMOCK_VERIFY(mockDelegate);

  FakeMagicStackRankingModelDelegate* fakeDelegate =
      [[FakeMagicStackRankingModelDelegate alloc] init];
  _magicStackRankingModel.delegate = fakeDelegate;
  [_magicStackRankingModel fetchLatestMagicStackRanking];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), true, ^bool() {
        base::RunLoop().RunUntilIdle();
        return [fakeDelegate.rank count] > 0;
      }));

  _magicStackRankingModel.delegate = mockDelegate;
  OCMExpect([mockDelegate magicStackRankingModel:[OCMArg any]
                                   didRemoveItem:[OCMArg any]]);
  [_magicStackRankingModel removeSafetyCheckModule];
  EXPECT_OCMOCK_VERIFY(mockDelegate);
}

// Test that disabling the Magic Stack ranking model doesn't crash and doesn't
// perform a valid fetch.
TEST_F(MagicStackRankingModelTest, TestDisabledSegmentationRanking) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {}, {{segmentation_platform::features::
                kSegmentationPlatformIosModuleRanker}});
  id mockDelegate =
      OCMStrictProtocolMock(@protocol(MagicStackRankingModelDelegate));
  _magicStackRankingModel.delegate = mockDelegate;
  OCMReject([mockDelegate magicStackRankingModel:[OCMArg any]
                        didGetLatestRankingOrder:[OCMArg any]]);
  [_magicStackRankingModel fetchLatestMagicStackRanking];
  base::RunLoop().RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mockDelegate);
}

// Test that fetching the ephemeral card to show in the Magic Stack returns the
// correct card.
TEST_F(MagicStackRankingModelTest, TestEphemeralModelDidGetCardToShow) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{commerce::kPriceTrackingPromo, {}},
       {segmentation_platform::features::
            kSegmentationPlatformEphemeralCardRanker,
        {{segmentation_platform::features::
              kEphemeralCardRankerForceShowCardParam,
          segmentation_platform::features::kPriceTrackingPromoForceOverride}}}},
      {});
  commerce::MockShoppingService* shopping_service =
      static_cast<commerce::MockShoppingService*>(
          commerce::ShoppingServiceFactory::GetForProfile(GetProfile()));
  shopping_service->SetIsShoppingListEligible(true);

  FakeMagicStackRankingModelDelegate* delegate_ =
      [[FakeMagicStackRankingModelDelegate alloc] init];
  _magicStackRankingModel.delegate = delegate_;
  [_magicStackRankingModel fetchLatestMagicStackRanking];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), true, ^bool() {
        base::RunLoop().RunUntilIdle();
        return [delegate_.rank count] > 0 &&
               _magicStackRankingModel.hasReceivedMagicStackResponse &&
               _magicStackRankingModel.hasReceivedEphemericalCardResponse;
      }));
  NSArray* expectedModuleRank = @[ @(5), @(15), @(1), @(10), @(11) ];
  EXPECT_EQ([delegate_.rank count], [expectedModuleRank count]);
  for (NSUInteger i = 0; i < [expectedModuleRank count]; i++) {
    MagicStackModule* config = delegate_.rank[i];
    EXPECT_EQ(@(int(config.type)), expectedModuleRank[i])
        << "For Magic Stack order index " << i;
  }
}
