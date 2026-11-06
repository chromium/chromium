// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/coordinator/most_visited_tiles_mediator.h"

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/history/core/browser/history_service.h"
#import "components/history/core/test/history_service_test_util.h"
#import "components/ntp_tiles/icon_cacher.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/ntp_tiles/ntp_tile.h"
#import "components/ntp_tiles/section_type.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
#import "components/search_engines/ai_mode_button_service.h"
#import "components/search_engines/template_url_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios.h"
#import "ios/chrome/browser/content_suggestions/model/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tile_view.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_actions_provider.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_consumer.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Faked most visited sites for testing purpose. This class exposes the
// observer so the test could call observer methods with desired parameters.
class FakeMostVisitedSites : public ntp_tiles::MostVisitedSites {
 public:
  // Define a callback type that helps capture the Observer pointer.
  using ObserverCallback =
      base::RepeatingCallback<void(ntp_tiles::MostVisitedSites::Observer*)>;

  FakeMostVisitedSites(
      PrefService* prefs,
      signin::IdentityManager* identity_manager,
      supervised_user::SupervisedUserService* supervised_user_service,
      supervised_user::SupervisedUserUrlFilteringService*
          supervised_user_url_filtering_service,
      scoped_refptr<history::TopSites> top_sites,
      std::unique_ptr<ntp_tiles::PopularSites> popular_sites,
      std::unique_ptr<ntp_tiles::CustomLinksManager> custom_links,
      std::unique_ptr<ntp_tiles::EnterpriseShortcutsManager>
          enterprise_shortcuts,
      std::unique_ptr<ntp_tiles::IconCacher> icon_cacher,
      bool is_default_chrome_app_migrated)
      : MostVisitedSites(prefs,
                         identity_manager,
                         supervised_user_service,
                         supervised_user_url_filtering_service,
                         top_sites,
                         std::move(popular_sites),
                         std::move(custom_links),
                         std::move(enterprise_shortcuts),
                         std::move(icon_cacher),
                         is_default_chrome_app_migrated) {}

  ~FakeMostVisitedSites() override = default;

  void AddMostVisitedURLsObserver(
      Observer* observer,
      size_t num_sites,
      std::optional<size_t> max_num_non_custom_sites) override {
    if (observer_registration_callback_) {
      observer_registration_callback_.Run(observer);
    }
  }

  // Setter for the test to inject the logic.
  void SetObserverRegistrationCallback(ObserverCallback callback) {
    observer_registration_callback_ = std::move(callback);
  }

 private:
  ObserverCallback observer_registration_callback_;
};

}  // namespace

@interface MostVisitedTilesMediator (Testing)
- (void)pinOrUnpinMostVisited:(MostVisitedItem*)item;
- (void)undoUnpinAIMTileWithIndex:(int)oldIndex;
@end

// Testing Suite for MostVisitedTilesMediator.
class MostVisitedTilesMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(test_profile_builder).Build();

    RegisterProfilePrefs(pref_service_.registry());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    tracker_ = std::make_unique<feature_engagement::test::MockTracker>();
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
    delegate_ = OCMProtocolMock(@protocol(NewTabPageActionsDelegate));
  }

  // Initializes `ai_mode_button_service_ios_` and its backing dependencies with
  // `initial_eligibility`.
  void SetUpAIModeButtonService(bool initial_eligibility) {
    is_aim_eligible_ = initial_eligibility;
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service->Load();

    mock_aim_eligibility_service_ =
        std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
            pref_service_, /*template_url_service=*/nullptr,
            /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);
    EXPECT_CALL(*mock_aim_eligibility_service_,
                RegisterEligibilityChangedCallback(testing::_))
        .WillOnce([this](base::RepeatingClosure callback) {
          aim_eligibility_callback_ = std::move(callback);
          return base::CallbackListSubscription();
        });
    ON_CALL(*mock_aim_eligibility_service_, IsAimEligible())
        .WillByDefault([this]() { return is_aim_eligible_; });

    ai_mode_button_service_ =
        std::make_unique<AiModeButtonService>(template_url_service);
    ai_mode_button_service_ios_ = std::make_unique<AIModeButtonServiceIOS>(
        template_url_service, mock_aim_eligibility_service_.get(),
        ai_mode_button_service_.get(), /*favicon_loader=*/nullptr,
        /*favicon_service=*/nullptr, /*image_fetcher_service=*/nullptr);
  }

  void CreateMediator(
      AIModeButtonServiceIOS* ai_mode_button_service = nullptr) {
    CHECK(!mediator_);

    history::HistoryService* history_service =
        ios::HistoryServiceFactory::GetForProfile(profile_.get());
    favicon::LargeIconService* large_icon_service =
        IOSChromeLargeIconServiceFactory::GetForProfile(profile_.get());
    LargeIconCache* cache =
        IOSChromeLargeIconCacheFactory::GetForProfile(profile_.get());
    std::unique_ptr<FakeMostVisitedSites> most_visited_sites =
        std::make_unique<FakeMostVisitedSites>(
            &pref_service_, /*identity_manager=*/nullptr,
            /*supervised_user_service=*/nullptr,
            /*supervised_user_url_filtering_service=*/nullptr,
            /*top_sites=*/nullptr, /*popular_sites=*/nullptr,
            /*custom_links=*/nullptr, /*enterprise_shortcuts=*/nullptr,
            /*icon_cacher=*/nullptr,
            /*is_default_chrome_app_migrated=*/true);
    most_visited_sites->SetObserverRegistrationCallback(base::BindRepeating(
        &MostVisitedTilesMediatorTest::set_captured_observer,
        base::Unretained(this)));

    mediator_ = [[MostVisitedTilesMediator alloc]
        initWithMostVisitedSite:std::move(most_visited_sites)
                 historyService:history_service
                    prefService:&pref_service_
               largeIconService:large_icon_service
                 largeIconCache:cache
         URLLoadingBrowserAgent:url_loader_
          accountManagerService:nullptr
              engagementTracker:tracker_.get()
              layoutGuideCenter:nil
            aiModeButtonService:ai_mode_button_service];

    mediator_.actionFactory = mock_action_factory();
    mediator_.NTPActionsDelegate = delegate_;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)delegate_);
    set_captured_observer(nullptr);
    [mediator_ disconnect];
    mediator_ = nil;
    if (ai_mode_button_service_ios_) {
      ai_mode_button_service_ios_->Shutdown();
    }
    ai_mode_button_service_ios_.reset();
    ai_mode_button_service_.reset();
    mock_aim_eligibility_service_.reset();
    PlatformTest::TearDown();
  }

  ~MostVisitedTilesMediatorTest() override = default;

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  void set_captured_observer(ntp_tiles::MostVisitedSites::Observer* observer) {
    captured_observer_ = observer;
  }

  id mock_action_factory() {
    id mock_action_factory = OCMClassMock([BrowserActionFactory class]);
    UIAction* dummy_action = [UIAction actionWithTitle:@""
                                                 image:nil
                                            identifier:nil
                                               handler:^(UIAction* action){
                                               }];
    OCMStub([mock_action_factory actionToOpenInNewTabWithBlock:[OCMArg any]])
        .andReturn(dummy_action);
    OCMStub([mock_action_factory
                actionToOpenInNewIncognitoTabWithBlock:[OCMArg any]])
        .andReturn(dummy_action);
    OCMStub([[mock_action_factory ignoringNonObjectArgs]
                actionToOpenInNewWindowWithURL:GURL()
                                activityOrigin:WindowActivityUnknownOrigin])
        .andReturn(dummy_action);
    OCMStub([mock_action_factory actionToCopyURL:[OCMArg any]])
        .andReturn(dummy_action);
    OCMStub([mock_action_factory actionToShareWithBlock:[OCMArg any]])
        .andReturn(dummy_action);
    OCMStub([mock_action_factory
                actionToEditPinnedSiteOnMostVisitedTileWithBlock:[OCMArg any]])
        .andReturn(dummy_action);
    OCMStub([mock_action_factory
                actionToUnpinSiteFromMostVisitedTileWithBlock:[OCMArg any]])
        .andReturn(dummy_action);
    OCMStub([mock_action_factory
                actionToPinSiteToMostVisitedTileWithBlock:[OCMArg any]])
        .andReturn(dummy_action);
    OCMStub([mock_action_factory actionToRemoveWithBlock:[OCMArg any]])
        .andReturn(dummy_action);
    return mock_action_factory;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<feature_engagement::test::MockTracker> tracker_;
  bool is_aim_eligible_ = false;
  base::RepeatingClosure aim_eligibility_callback_;
  std::unique_ptr<testing::NiceMock<MockAimEligibilityService>>
      mock_aim_eligibility_service_;
  std::unique_ptr<AiModeButtonService> ai_mode_button_service_;
  std::unique_ptr<AIModeButtonServiceIOS> ai_mode_button_service_ios_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  MostVisitedTilesMediator* mediator_;
  id<NewTabPageActionsDelegate> delegate_;
  // The captured observer, ready to be used in tests
  raw_ptr<ntp_tiles::MostVisitedSites::Observer> captured_observer_;
};

// Tests that the command is sent to the loader when opening a most visited.
TEST_F(MostVisitedTilesMediatorTest, TestOpenMostVisited) {
  CreateMediator();
  GURL url = GURL("http://chromium.org");
  MostVisitedItem* item = [[MostVisitedItem alloc] init];
  item.URL = url;
  MostVisitedTileView* view =
      [[MostVisitedTileView alloc] initWithConfiguration:item];
  UIGestureRecognizer* recognizer = [[UIGestureRecognizer alloc] init];
  [view addGestureRecognizer:recognizer];
  OCMExpect([mediator_.NTPActionsDelegate mostVisitedTileOpened]);

  // Action.
  [mediator_ mostVisitedTileTapped:recognizer];

  // Test.
  EXPECT_EQ(url, url_loader_->last_params.web_params.url);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      url_loader_->last_params.web_params.transition_type));
}

// Tests that when other conditions are met, the in-product help for pinning a
// site to the most visited tile is attempted if the user has visited a top site
// enough times.
TEST_F(MostVisitedTilesMediatorTest, TestPinSiteInProductHelpCondition) {
  CreateMediator();
  EXPECT_CALL(*tracker_,
              WouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSPinMostVisitedSiteFeature)))
      .WillRepeatedly(testing::Return(true));

  // Setup the URL and history service.
  GURL kUrl("https://www.chromium.org/");
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(profile_.get());

  // Fake two recent visits, and one for more than a week ago, in chronological
  // order.
  history_service->AddPage(kUrl, base::Time::Now() - base::Days(8),
                           history::SOURCE_BROWSED);
  history_service->AddPage(kUrl, base::Time::Now() - base::Seconds(20),
                           history::SOURCE_BROWSED);
  history_service->AddPage(kUrl, base::Time::Now() - base::Seconds(10),
                           history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);

  // Create a section with the NTPTile with source TOP_SITES.
  ntp_tiles::NTPTile tile;
  tile.title = u"Chromium";
  tile.url = kUrl;
  tile.source = ntp_tiles::TileSource::TOP_SITES;
  ntp_tiles::NTPTilesVector tiles_vector;
  tiles_vector.push_back(tile);
  std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector> sections;
  sections[ntp_tiles::SectionType::PERSONALIZED] = tiles_vector;
  ASSERT_TRUE(captured_observer_);

  // Check that the in-product help will not be triggered.
  id help_handler = OCMProtocolMock(@protocol(HelpCommands));
  mediator_.helpHandler = help_handler;
  OCMReject([help_handler
      presentInProductHelpWithType:InProductHelpType::kPinSiteToMostVisited]);
  captured_observer_->OnURLsAvailable(/*is_user_triggered=*/true, sections);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
  EXPECT_OCMOCK_VERIFY(help_handler);

  // Add a third recent entry to history, and verify that the in-product help
  // gets triggered.
  help_handler = OCMProtocolMock(@protocol(HelpCommands));
  mediator_.helpHandler = help_handler;
  OCMExpect([help_handler
      presentInProductHelpWithType:InProductHelpType::kPinSiteToMostVisited]);
  history_service->AddPage(kUrl, base::Time::Now(), history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
  captured_observer_->OnURLsAvailable(/*is_user_triggered=*/true, sections);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
  EXPECT_OCMOCK_VERIFY(help_handler);
}

// Tests that accessibility custom actions for pinned items include move actions
// when customization is enabled.
TEST_F(MostVisitedTilesMediatorTest, TestAccessibilityCustomActionsMove) {
  CreateMediator();
  // Create a list of tiles with one pinned site and one top site.
  ntp_tiles::NTPTile pinnedTile;
  pinnedTile.title = u"Pinned";
  pinnedTile.url = GURL("https://pinned.com");
  pinnedTile.source = ntp_tiles::TileSource::CUSTOM_LINKS;

  ntp_tiles::NTPTile topTile;
  topTile.title = u"Top";
  topTile.url = GURL("https://top.com");
  topTile.source = ntp_tiles::TileSource::TOP_SITES;

  ntp_tiles::NTPTilesVector tiles_vector;
  tiles_vector.push_back(pinnedTile);
  tiles_vector.push_back(topTile);

  std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector> sections;
  sections[ntp_tiles::SectionType::PERSONALIZED] = tiles_vector;

  // Trigger tile update to populate _mostVisitedConfig.
  captured_observer_->OnURLsAvailable(/*is_user_triggered=*/true, sections);
  MostVisitedTilesConfig* config = mediator_.mostVisitedConfig;
  ASSERT_EQ(2U, config.mostVisitedItems.count);
  MostVisitedItem* item0 = config.mostVisitedItems[0];
  EXPECT_TRUE(item0.isPinned);
  NSArray<UIAccessibilityCustomAction*>* actions =
      [item0.actionsProvider accessibilityCustomActionsForItem:item0
                                                      fromView:nil];
  // In LTR, at index 0, it should NOT have "Move Left" but should have "Move
  // Right" if the next is pinned.
  // In this example, the next site is NOT pinned. So it should have NO move
  // actions.
  BOOL foundMoveLeft = NO;
  BOOL foundMoveRight = NO;
  for (UIAccessibilityCustomAction* action in actions) {
    if ([action.name
            isEqualToString:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_SUGGESTIONS_MOVE_LEFT)]) {
      foundMoveLeft = YES;
    }
    if ([action.name
            isEqualToString:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_SUGGESTIONS_MOVE_RIGHT)]) {
      foundMoveRight = YES;
    }
  }
  EXPECT_FALSE(foundMoveLeft);
  EXPECT_FALSE(foundMoveRight);

  // Now test with two pinned sites.
  tiles_vector.clear();
  tiles_vector.push_back(pinnedTile);
  tiles_vector.push_back(pinnedTile);  // Another pinned site
  sections[ntp_tiles::SectionType::PERSONALIZED] = tiles_vector;
  captured_observer_->OnURLsAvailable(/*is_user_triggered=*/true, sections);
  config = mediator_.mostVisitedConfig;

  // Test first item; it should only be able to move right.
  item0 = config.mostVisitedItems[0];
  actions = [item0.actionsProvider accessibilityCustomActionsForItem:item0
                                                            fromView:nil];
  foundMoveLeft = NO;
  foundMoveRight = NO;
  for (UIAccessibilityCustomAction* action in actions) {
    if ([action.name
            isEqualToString:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_SUGGESTIONS_MOVE_LEFT)]) {
      foundMoveLeft = YES;
    }
    if ([action.name
            isEqualToString:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_SUGGESTIONS_MOVE_RIGHT)]) {
      foundMoveRight = YES;
    }
  }
  EXPECT_FALSE(foundMoveLeft);
  EXPECT_TRUE(foundMoveRight);

  // Test second item; it should only be able to move left.
  MostVisitedItem* item1 = config.mostVisitedItems[1];
  actions = [item1.actionsProvider accessibilityCustomActionsForItem:item1
                                                            fromView:nil];
  foundMoveLeft = NO;
  foundMoveRight = NO;
  for (UIAccessibilityCustomAction* action in actions) {
    if ([action.name
            isEqualToString:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_SUGGESTIONS_MOVE_LEFT)]) {
      foundMoveLeft = YES;
    }
    if ([action.name
            isEqualToString:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_SUGGESTIONS_MOVE_RIGHT)]) {
      foundMoveRight = YES;
    }
  }
  EXPECT_TRUE(foundMoveLeft);
  EXPECT_FALSE(foundMoveRight);
}

// Tests that setConsumer delivers cached config when NTP Redesign is enabled.
TEST_F(MostVisitedTilesMediatorTest,
       TestSetConsumerDeliversConfigWhenNTPRedesignEnabled) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  feature_list_.InitAndEnableFeature(kNewTabPageRedesign);
  CreateMediator();

  ntp_tiles::NTPTilesVector tiles_vector;
  ntp_tiles::NTPTile tile;
  tile.url = GURL("http://chromium.org");
  tile.title = u"Chromium";
  tiles_vector.push_back(tile);
  std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector> sections;
  sections[ntp_tiles::SectionType::PERSONALIZED] = tiles_vector;
  captured_observer_->OnURLsAvailable(/*is_user_triggered=*/true, sections);

  id mock_consumer = OCMProtocolMock(@protocol(ContentSuggestionsConsumer));
  OCMExpect([mock_consumer setMostVisitedTilesConfig:[OCMArg any]]);

  mediator_.consumer = mock_consumer;

  EXPECT_OCMOCK_VERIFY(mock_consumer);
}

// Tests that when `mostVisitedSites:didUpdateTiles:` arrives on cold start
// before `AIModeButtonServiceIOS` becomes available, the virtual AI Mode tile
// is inserted once the service state updates, and removed if eligibility is
// later revoked.
TEST_F(MostVisitedTilesMediatorTest,
       TestAIMTileInsertedOnColdStartWhenServiceBecomesAvailable) {
  int arm = static_cast<int>(AimButtonRefactorArm::kAimAsMvt);
  feature_list_.InitAndEnableFeatureWithParameters(
      kAimButtonRefactor,
      {{kAimButtonRefactorArmParam, base::NumberToString(arm)}});

  SetUpAIModeButtonService(/*initial_eligibility=*/false);
  EXPECT_FALSE(ai_mode_button_service_ios_->IsButtonAvailable());

  CreateMediator(ai_mode_button_service_ios_.get());
  ASSERT_TRUE(captured_observer_);

  id mock_consumer = OCMProtocolMock(@protocol(ContentSuggestionsConsumer));
  mediator_.consumer = mock_consumer;

  base::HistogramTester histogram_tester;

  // Deliver initial tiles while AIM eligibility is still false.
  ntp_tiles::NTPTile top_tile;
  top_tile.title = u"Chromium";
  top_tile.url = GURL("https://www.chromium.org/");
  top_tile.source = ntp_tiles::TileSource::TOP_SITES;
  std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector> sections;
  sections[ntp_tiles::SectionType::PERSONALIZED] = {top_tile};
  OCMExpect([mock_consumer setMostVisitedTilesConfig:[OCMArg any]]);
  captured_observer_->OnURLsAvailable(/*is_user_triggered=*/false, sections);
  EXPECT_OCMOCK_VERIFY(mock_consumer);

  ASSERT_EQ(1U, mediator_.mostVisitedConfig.mostVisitedItems.count);
  EXPECT_EQ(ntp_tiles::TileSource::TOP_SITES,
            mediator_.mostVisitedConfig.mostVisitedItems[0].source);
  histogram_tester.ExpectUniqueSample("Omnibox.AimEntrypoint.Shown", false, 1);

  // Simulate AIM eligibility becoming true after initial tiles have loaded,
  // without a new `OnURLsAvailable` call from `MostVisitedSites`.
  is_aim_eligible_ = true;
  ASSERT_TRUE(aim_eligibility_callback_);
  OCMExpect([mock_consumer setMostVisitedTilesConfig:[OCMArg any]]);
  aim_eligibility_callback_.Run();
  EXPECT_OCMOCK_VERIFY(mock_consumer);
  EXPECT_TRUE(ai_mode_button_service_ios_->IsButtonAvailable());

  ASSERT_EQ(2U, mediator_.mostVisitedConfig.mostVisitedItems.count);
  EXPECT_EQ(ntp_tiles::TileSource::AI_MODE,
            mediator_.mostVisitedConfig.mostVisitedItems[0].source);
  EXPECT_EQ(0, mediator_.mostVisitedConfig.mostVisitedItems[0].index);
  EXPECT_EQ(ntp_tiles::TileSource::TOP_SITES,
            mediator_.mostVisitedConfig.mostVisitedItems[1].source);
  EXPECT_EQ(1, mediator_.mostVisitedConfig.mostVisitedItems[1].index);
  histogram_tester.ExpectBucketCount("Omnibox.AimEntrypoint.Shown", false, 1);
  histogram_tester.ExpectBucketCount("Omnibox.AimEntrypoint.Shown", true, 1);

  // Simulate AIM eligibility becoming false; the AIM tile should be removed
  // and remaining items re-indexed.
  is_aim_eligible_ = false;
  OCMExpect([mock_consumer setMostVisitedTilesConfig:[OCMArg any]]);
  aim_eligibility_callback_.Run();
  EXPECT_OCMOCK_VERIFY(mock_consumer);

  ASSERT_EQ(1U, mediator_.mostVisitedConfig.mostVisitedItems.count);
  EXPECT_EQ(ntp_tiles::TileSource::TOP_SITES,
            mediator_.mostVisitedConfig.mostVisitedItems[0].source);
  EXPECT_EQ(0, mediator_.mostVisitedConfig.mostVisitedItems[0].index);
  histogram_tester.ExpectBucketCount("Omnibox.AimEntrypoint.Shown", false, 2);
  histogram_tester.ExpectBucketCount("Omnibox.AimEntrypoint.Shown", true, 1);
}

// Tests that unpinning the virtual AI Mode tile immediately restores a
// suppressed zero-state AIM URL from `TOP_SITES`, avoids redundant updates on
// eligibility changes while unpinned, and re-inserts the tile on undo.
TEST_F(MostVisitedTilesMediatorTest,
       TestAIMTileUnpinAndUndoRestoresSuppressedTopSite) {
  feature_list_.InitWithFeaturesAndParameters(
      {{kAimButtonRefactor,
        {{kAimButtonRefactorArmParam, base::NumberToString(static_cast<int>(
                                          AimButtonRefactorArm::kAimAsMvt))}}},
       {kAIMNTPEntrypointTablet, {}}},
      {});

  SetUpAIModeButtonService(/*initial_eligibility=*/true);

  CreateMediator(ai_mode_button_service_ios_.get());
  ASSERT_TRUE(captured_observer_);

  base::HistogramTester histogram_tester;

  // Deliver a natural zero-state AIM URL in `TOP_SITES` plus a normal top site.
  ntp_tiles::NTPTile aim_top_tile;
  aim_top_tile.title = u"Google AI Mode";
  aim_top_tile.url = GURL("https://www.google.com/search?udm=50");
  aim_top_tile.source = ntp_tiles::TileSource::TOP_SITES;

  ntp_tiles::NTPTile regular_top_tile;
  regular_top_tile.title = u"Chromium";
  regular_top_tile.url = GURL("https://www.chromium.org/");
  regular_top_tile.source = ntp_tiles::TileSource::TOP_SITES;

  std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector> sections;
  sections[ntp_tiles::SectionType::PERSONALIZED] = {aim_top_tile,
                                                    regular_top_tile};
  captured_observer_->OnURLsAvailable(/*is_user_triggered=*/false, sections);

  // While the virtual AIM tile is shown, the natural zero-state AIM URL in
  // `TOP_SITES` is suppressed.
  ASSERT_EQ(2U, mediator_.mostVisitedConfig.mostVisitedItems.count);
  EXPECT_EQ(ntp_tiles::TileSource::AI_MODE,
            mediator_.mostVisitedConfig.mostVisitedItems[0].source);
  EXPECT_EQ(regular_top_tile.url,
            mediator_.mostVisitedConfig.mostVisitedItems[1].URL);
  histogram_tester.ExpectUniqueSample("Omnibox.AimEntrypoint.Shown", true, 1);

  // Unpin the virtual AIM tile; the suppressed `TOP_SITES` zero-state AIM tile
  // should be immediately restored from `_lastMostVisitedTiles`.
  [mediator_
      pinOrUnpinMostVisited:mediator_.mostVisitedConfig.mostVisitedItems[0]];
  EXPECT_EQ(-1,
            pref_service_.GetInteger(prefs::kIosMostVisitedAiModeTileIndex));
  ASSERT_EQ(2U, mediator_.mostVisitedConfig.mostVisitedItems.count);
  EXPECT_EQ(ntp_tiles::TileSource::TOP_SITES,
            mediator_.mostVisitedConfig.mostVisitedItems[0].source);
  EXPECT_EQ(aim_top_tile.url,
            mediator_.mostVisitedConfig.mostVisitedItems[0].URL);
  EXPECT_EQ(regular_top_tile.url,
            mediator_.mostVisitedConfig.mostVisitedItems[1].URL);

  // Toggling eligibility while the AIM tile is unpinned should not rebuild or
  // log extra `Omnibox.AimEntrypoint.Shown` samples.
  is_aim_eligible_ = false;
  aim_eligibility_callback_.Run();
  is_aim_eligible_ = true;
  aim_eligibility_callback_.Run();
  histogram_tester.ExpectUniqueSample("Omnibox.AimEntrypoint.Shown", true, 1);

  // Undo unpinning the virtual AIM tile; it should be re-inserted at index 0
  // and suppress the natural zero-state AIM `TOP_SITES` tile again.
  [mediator_ undoUnpinAIMTileWithIndex:0];
  ASSERT_EQ(2U, mediator_.mostVisitedConfig.mostVisitedItems.count);
  EXPECT_EQ(ntp_tiles::TileSource::AI_MODE,
            mediator_.mostVisitedConfig.mostVisitedItems[0].source);
  EXPECT_EQ(regular_top_tile.url,
            mediator_.mostVisitedConfig.mostVisitedItems[1].URL);
}
