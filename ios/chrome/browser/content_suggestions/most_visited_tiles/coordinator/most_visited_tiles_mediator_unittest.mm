// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/coordinator/most_visited_tiles_mediator.h"

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/history/core/browser/history_service.h"
#import "components/history/core/test/history_service_test_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/ntp_tiles/icon_cacher.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/ntp_tiles/ntp_tile.h"
#import "components/ntp_tiles/section_type.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/content_suggestions/model/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tile_view.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
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

// Testing Suite for MostVisitedTilesMediator.
class MostVisitedTilesMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        kMostVisitedTilesCustomizationIOS);
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());
    profile_ = std::move(test_profile_builder).Build();

    history::HistoryService* history_service =
        ios::HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    favicon::LargeIconService* large_icon_service =
        IOSChromeLargeIconServiceFactory::GetForProfile(profile_.get());
    LargeIconCache* cache =
        IOSChromeLargeIconCacheFactory::GetForProfile(profile_.get());
    RegisterProfilePrefs(pref_service_.registry());
    std::unique_ptr<FakeMostVisitedSites> most_visited_sites =
        std::make_unique<FakeMostVisitedSites>(
            &pref_service_, /*identity_manager=*/nullptr,
            /*supervised_user_service=*/nullptr,
            /*supervised_user_url_filtering_service=*/nullptr,
            /*top_sites=*/nullptr,
            /*popular_sites=*/nullptr,
            /*custom_links=*/nullptr, /*enterprise_shortcuts*/ nullptr,
            /*icon_cacher=*/nullptr,
            /*is_default_chrome_app_migrated=*/true);
    most_visited_sites->SetObserverRegistrationCallback(base::BindRepeating(
        &MostVisitedTilesMediatorTest::set_captured_observer,
        base::Unretained(this)));

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    tracker_ = std::make_unique<feature_engagement::test::MockTracker>();
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    mediator_ = [[MostVisitedTilesMediator alloc]
        initWithMostVisitedSite:std::move(most_visited_sites)
                 historyService:history_service
                    prefService:&pref_service_
               largeIconService:large_icon_service
                 largeIconCache:cache
         URLLoadingBrowserAgent:url_loader_
          accountManagerService:nullptr
              engagementTracker:tracker_.get()
              layoutGuideCenter:nil];

    metrics_recorder_ = [[ContentSuggestionsMetricsRecorder alloc]
        initWithLocalState:local_state()];
    mediator_.contentSuggestionsMetricsRecorder = metrics_recorder_;
    delegate_ = OCMProtocolMock(@protocol(NewTabPageActionsDelegate));
    mediator_.NTPActionsDelegate = delegate_;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)delegate_);
    set_captured_observer(nullptr);
    PlatformTest::TearDown();
  }

  ~MostVisitedTilesMediatorTest() override { [mediator_ disconnect]; }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  void set_captured_observer(ntp_tiles::MostVisitedSites::Observer* observer) {
    captured_observer_ = observer;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<feature_engagement::test::MockTracker> tracker_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  MostVisitedTilesMediator* mediator_;
  id<NewTabPageActionsDelegate> delegate_;
  ContentSuggestionsMetricsRecorder* metrics_recorder_;
  // The captured observer, ready to be used in tests
  raw_ptr<ntp_tiles::MostVisitedSites::Observer> captured_observer_;
};

// Tests that the command is sent to the loader when opening a most visited.
TEST_F(MostVisitedTilesMediatorTest, TestOpenMostVisited) {
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
  EXPECT_CALL(*tracker_,
              WouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSPinMostVisitedSiteFeature)))
      .WillRepeatedly(testing::Return(true));

  // Setup the URL and history service.
  GURL kUrl("https://www.chromium.org/");
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);

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
