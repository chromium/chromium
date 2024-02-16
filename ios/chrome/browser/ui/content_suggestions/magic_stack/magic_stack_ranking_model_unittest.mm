// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model.h"

#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_timeouts.h"
#import "base/threading/thread_restrictions.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_mediator.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using set_up_list_prefs::SetUpListItemState;
using startup_metric_utils::FirstRunSentinelCreationResult;

#define EXPECT_SET_MAGIC_STACK_ORDER(consumer, ...)                      \
  {                                                                      \
    id block_checker = [OCMArg checkWithBlock:^BOOL(id value) {          \
      NSArray<NSNumber*>* magic_stack_order = (NSArray*)value;           \
      std::vector<ContentSuggestionsModuleType> expected_order = {       \
          __VA_ARGS__};                                                  \
      EXPECT_EQ(magic_stack_order.count, expected_order.size());         \
      for (unsigned int i = 0; i < expected_order.size(); i++) {         \
        EXPECT_EQ(magic_stack_order[i].intValue, int(expected_order[i])) \
            << "For Magic Stack order index " << i;                      \
      }                                                                  \
      return YES;                                                        \
    }];                                                                  \
    OCMExpect([consumer setMagicStackOrder:block_checker]);              \
  }

// Expose -addConsumer: to validate consumer calls.
@interface SetUpListMediator (Testing)
- (void)addConsumer:(id<SetUpListConsumer>)consumer;
@end

// Fake subclass of SetUpListMediator to easily allow for Set Up List to be
// shown.
@interface FakeSetUpListMediator : SetUpListMediator
@end

@implementation FakeSetUpListMediator

- (BOOL)shouldShowSetUpList {
  return YES;
}

@end

// Fake subclass of ParcelTrackingMediator to override item config construction.
@interface FakeParcelTrackingMediator : ParcelTrackingMediator
@end

@implementation FakeParcelTrackingMediator
- (ParcelTrackingItem*)parcelTrackingItemToShow {
  return [[ParcelTrackingItem alloc] init];
}
@end

// Fake subclass of TabResumptionMediator to override item config construction.
@interface FakeTabResumptionMediator : TabResumptionMediator
@end

@implementation FakeTabResumptionMediator

- (TabResumptionItem*)itemConfig {
  return [[TabResumptionItem alloc] initWithItemType:kMostRecentTab];
}

- (void)fetchLastTabResumptionItem {
}

@end

// Expose -hasReceivedMagicStackResponse for waiting for ranking to return.
@interface MagicStackRankingModel (Testing)
@property(nonatomic, assign, readonly) BOOL hasReceivedMagicStackResponse;
@end

// Testing Suite for MagicStackRankingModel
class MagicStackRankingModelTest : public PlatformTest {
 public:
  void SetUp() override {
    // Need to initialize features before constructing
    // SegmentationPlatformServiceFactory.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{segmentation_platform::features::kSegmentationPlatformFeature, {}},
         {segmentation_platform::features::kSegmentationPlatformIosModuleRanker,
          {{segmentation_platform::kDefaultModelEnabledParam, "true"}}},
         {kMagicStack, {{kMagicStackMostVisitedModuleParam, "true"}}},
         {kIOSParcelTracking, {}},
         {kTabResumption, {}}},
        {});

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(AuthenticationServiceFactory::GetDefaultFactory()));
    test_cbs_builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    // Necessary set up for kIOSSetUpList.
    local_state_.Get()->ClearPref(set_up_list_prefs::kDisabled);
    ClearDefaultBrowserPromoData();
    WriteFirstRunSentinel();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForBrowserState(chrome_browser_state_.get());
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(chrome_browser_state_.get());

    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());

    _setUpListMediator = [[FakeSetUpListMediator alloc]
          initWithPrefService:chrome_browser_state_.get()->GetPrefs()
                  syncService:syncService
              identityManager:identityManager
        authenticationService:authenticationService
                   sceneState:scene_state_];
    _parcelTrackingMediator = [[FakeParcelTrackingMediator alloc]
        initWithShoppingService:shopping_service_.get()
         URLLoadingBrowserAgent:url_loader_];
    _tabResumptionMediator = [[FakeTabResumptionMediator alloc]
        initWithLocalState:local_state_.Get()
               prefService:chrome_browser_state_.get()->GetPrefs()
           identityManager:identityManager
                   browser:browser_.get()];
    _magicStackRankingModel = [[MagicStackRankingModel alloc]
        initWithSegmentationService:
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetForBrowserState(chrome_browser_state_.get())
                        prefService:chrome_browser_state_.get()->GetPrefs()
                         localState:local_state_.Get()
                    moduleMediators:@[
                      _setUpListMediator, _parcelTrackingMediator,
                      _tabResumptionMediator
                    ]];
    consumer_ = OCMProtocolMock(@protocol(ContentSuggestionsConsumer));
    _magicStackRankingModel.consumer = consumer_;

    metrics_recorder_ = [[ContentSuggestionsMetricsRecorder alloc]
        initWithLocalState:local_state_.Get()];
    _magicStackRankingModel.contentSuggestionsMetricsRecorder =
        metrics_recorder_;

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  ~MagicStackRankingModelTest() override {
    [_setUpListMediator disconnect];
    [_tabResumptionMediator disconnect];
    [_parcelTrackingMediator disconnect];
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
    EXPECT_FALSE(set_up_list_prefs::IsSetUpListDisabled(local_state_.Get()));
    EXPECT_FALSE(FirstRun::IsChromeFirstRun());
    EXPECT_TRUE(set_up_list_utils::IsSetUpListActive(local_state_.Get()));
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  IOSChromeScopedTestingLocalState local_state_;
  FakeSceneState* scene_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  FakeUrlLoadingBrowserAgent* url_loader_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  FakeSetUpListMediator* _setUpListMediator;
  FakeParcelTrackingMediator* _parcelTrackingMediator;
  FakeTabResumptionMediator* _tabResumptionMediator;
  MagicStackRankingModel* _magicStackRankingModel;
  id consumer_;
  id setUpListConsumer_;
  ContentSuggestionsMetricsRecorder* metrics_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests that the -setMagicStackOrder: consumer call is executed with the
// correct order when fetching from the SegmentationPlatformService.
// NOTE: do not add new module features to this test, this one ensures the
// mediator can handle the fetched module order and filter out any extra modules
// that aren't enabled/valid for a client. Add new modules to
// TestMagicStackOrderSegmentationServiceCallWithNewFeatures.
TEST_F(MagicStackRankingModelTest, TestMagicStackOrderSegmentationServiceCall) {
  EXPECT_SET_MAGIC_STACK_ORDER(
      consumer_, ContentSuggestionsModuleType::kCompactedSetUpList,
      ContentSuggestionsModuleType::kMostVisited,
      ContentSuggestionsModuleType::kShortcuts,
      ContentSuggestionsModuleType::kTabResumption,
      ContentSuggestionsModuleType::kParcelTracking);

  [_magicStackRankingModel fetchLatestMagicStackRanking];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), true, ^bool() {
        base::RunLoop().RunUntilIdle();
        return _magicStackRankingModel.hasReceivedMagicStackResponse;
      }));
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the -setMagicStackOrder: consumer call is executed with the
// correct order with new modules enabled when fetching from the
// SegmentationPlatformService with kHideIrrelevantModulesParam enabled. Since
// the new features are in the back of the order ranking, verify that they are
// ultimately not in the order passed to the consumer.
TEST_F(MagicStackRankingModelTest,
       TestMagicStackOrderSegmentationServiceCallWithNewFeaturesHidden) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{segmentation_platform::features::kSegmentationPlatformFeature, {}},
       {segmentation_platform::features::kSegmentationPlatformIosModuleRanker,
        {{segmentation_platform::kDefaultModelEnabledParam, "true"}}},
       {kMagicStack,
        {{kMagicStackMostVisitedModuleParam, "true"},
         {kHideIrrelevantModulesParam, "true"}}},
       {kSafetyCheckMagicStack, {}},
       {kTabResumption, {}}},
      {});

  EXPECT_SET_MAGIC_STACK_ORDER(
      consumer_, ContentSuggestionsModuleType::kCompactedSetUpList,
      ContentSuggestionsModuleType::kMostVisited,
      ContentSuggestionsModuleType::kShortcuts, );

  [_magicStackRankingModel fetchLatestMagicStackRanking];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), true, ^bool() {
        base::RunLoop().RunUntilIdle();
        return _magicStackRankingModel.hasReceivedMagicStackResponse;
      }));
  EXPECT_OCMOCK_VERIFY(consumer_);
}

TEST_F(MagicStackRankingModelTest, TestSetUpListConsumerCall) {
  setUpListConsumer_ = OCMStrictProtocolMock(@protocol(SetUpListConsumer));
  consumer_ = OCMStrictProtocolMock(@protocol(ContentSuggestionsConsumer));
  _magicStackRankingModel.consumer = consumer_;
  _setUpListMediator.consumer = consumer_;
  [_setUpListMediator addConsumer:setUpListConsumer_];
  OCMExpect([consumer_ showSetUpListModuleWithConfigs:[OCMArg any]]);
  OCMExpect([consumer_ setShortcutTilesConfig:[OCMArg any]]);
  OCMExpect([consumer_ showTabResumptionWithItem:[OCMArg any]]);

  [_magicStackRankingModel fetchLatestMagicStackRanking];

  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([setUpListConsumer_ setUpListItemDidComplete:[OCMArg any]
                                       allItemsCompleted:NO
                                              completion:[OCMArg any]]);
  set_up_list_prefs::MarkItemComplete(local_state_.Get(),
                                      SetUpListItemType::kSignInSync);
  OCMExpect([setUpListConsumer_ setUpListItemDidComplete:[OCMArg any]
                                       allItemsCompleted:NO
                                              completion:[OCMArg any]]);
  set_up_list_prefs::MarkItemComplete(local_state_.Get(),
                                      SetUpListItemType::kDefaultBrowser);
  OCMExpect([setUpListConsumer_ setUpListItemDidComplete:[OCMArg any]
                                       allItemsCompleted:YES
                                              completion:[OCMArg any]]);
  set_up_list_prefs::MarkItemComplete(local_state_.Get(),
                                      SetUpListItemType::kAutofill);
  EXPECT_OCMOCK_VERIFY(setUpListConsumer_);
}

// Tests that when the user changes the setting to disable signin, the
// SetUpList signin item is marked complete.
TEST_F(MagicStackRankingModelTest, TestOnServiceStatusChanged) {
  // Verify the initial state.
  SetUpListItemState item_state = set_up_list_prefs::GetItemState(
      local_state_.Get(), SetUpListItemType::kSignInSync);
  EXPECT_EQ(item_state, SetUpListItemState::kNotComplete);

  // Simulate the user disabling signin.
  chrome_browser_state_.get()->GetPrefs()->SetBoolean(prefs::kSigninAllowed,
                                                      false);
  // Verify that the signin item is complete.
  item_state = set_up_list_prefs::GetItemState(local_state_.Get(),
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
