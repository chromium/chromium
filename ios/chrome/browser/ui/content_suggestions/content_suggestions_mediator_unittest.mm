// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#import "base/memory/scoped_refptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/default_clock.h"
#import "components/favicon/core/large_icon_service_impl.h"
#import "components/favicon/core/test/mock_favicon_service.h"
#import "components/ntp_tiles/icon_cacher.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/set_up_list_prefs.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/reading_list_test_utils.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/query_suggestion_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using set_up_list_prefs::SetUpListItemState;

@protocol ContentSuggestionsMediatorDispatcher <BrowserCoordinatorCommands,
                                                SnackbarCommands>
@end

// Testing Suite for ContentSuggestionsMediator
class ContentSuggestionsMediatorTest : public PlatformTest {
 public:
  ContentSuggestionsMediatorTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(AuthenticationServiceFactory::GetDefaultFactory()));
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::vector<scoped_refptr<ReadingListEntry>>()));
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(AuthenticationServiceFactory::GetDefaultFactory()));
    chrome_browser_state_ = test_cbs_builder.Build();

    scoped_feature_list_.InitWithFeatures({kIOSSetUpList}, {});
    base::ScopedAllowBlockingForTesting allow_blocking;
    FirstRun::RemoveSentinel();
    base::File::Error fileError;
    FirstRun::CreateSentinel(&fileError);
    FirstRun::LoadSentinelInfo();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    large_icon_service_.reset(new favicon::LargeIconServiceImpl(
        &mock_favicon_service_, nullptr, 32, favicon_base::IconType::kTouchIcon,
        "test_chrome"));
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    web_state_list_ = browser_->GetWebStateList();
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    fake_web_state_->SetBrowserState(chrome_browser_state_.get());
    NewTabPageTabHelper::CreateForWebState(fake_web_state_.get());
    dispatcher_ =
        OCMProtocolMock(@protocol(ContentSuggestionsMediatorDispatcher));
    consumer_ = OCMProtocolMock(@protocol(ContentSuggestionsConsumer));
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.Get());
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

    favicon::LargeIconService* largeIconService =
        IOSChromeLargeIconServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    LargeIconCache* cache = IOSChromeLargeIconCacheFactory::GetForBrowserState(
        chrome_browser_state_.get());
    std::unique_ptr<ntp_tiles::MostVisitedSites> mostVisitedSites =
        std::make_unique<ntp_tiles::MostVisitedSites>(
            &pref_service_, /*top_sites*/ nullptr, /*popular_sites*/ nullptr,
            /*custom_links*/ nullptr, /*icon_cacher*/ nullptr,
            /*supervisor=*/nullptr, true);
    ntp_tiles::MostVisitedSites::RegisterProfilePrefs(pref_service_.registry());
    ReadingListModel* readingListModel =
        ReadingListModelFactory::GetForBrowserState(
            chrome_browser_state_.get());

    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());

    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(chrome_browser_state_.get());

    mediator_ = [[ContentSuggestionsMediator alloc]
             initWithLargeIconService:largeIconService
                       largeIconCache:cache
                      mostVisitedSite:std::move(mostVisitedSites)
                     readingListModel:readingListModel
                          prefService:chrome_browser_state_.get()->GetPrefs()
        isGoogleDefaultSearchProvider:NO
                authenticationService:authentication_service
                      identityManager:identityManager
                              browser:browser_.get()];
    mediator_.dispatcher = dispatcher_;
    mediator_.consumer = consumer_;
    mediator_.webStateList = browser_.get()->GetWebStateList();
    mediator_.webState = fake_web_state_.get();

    metrics_recorder_ = [[ContentSuggestionsMetricsRecorder alloc] init];
    mediator_.contentSuggestionsMetricsRecorder = metrics_recorder_;

    promos_manager_ = std::make_unique<MockPromosManager>();
    mediator_.promosManager = promos_manager_.get();

    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
    histogram_tester_.reset(new base::HistogramTester());
  }

  ~ContentSuggestionsMediatorTest() override { [mediator_ disconnect]; }

 protected:
  std::unique_ptr<web::FakeWebState> CreateWebState(const char* url) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    NewTabPageTabHelper::CreateForWebState(test_web_state.get());
    test_web_state->SetCurrentURL(GURL(url));
    test_web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    test_web_state->SetBrowserState(chrome_browser_state_.get());
    return test_web_state;
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  IOSChromeScopedTestingLocalState local_state_;
  SceneState* scene_state_;
  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  WebStateList* web_state_list_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  id dispatcher_;
  id consumer_;
  std::unique_ptr<favicon::LargeIconServiceImpl> large_icon_service_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  ContentSuggestionsMediator* mediator_;
  FakeUrlLoadingBrowserAgent* url_loader_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  ContentSuggestionsMetricsRecorder* metrics_recorder_;
};

// Tests that the command is sent to the dispatcher when opening the Reading
// List.
TEST_F(ContentSuggestionsMediatorTest, TestOpenReadingList) {
  OCMExpect([dispatcher_ showReadingList]);

  OCMExpect([mediator_.NTPMetricsDelegate shortcutTileOpened]);

  // Action.
  ContentSuggestionsMostVisitedActionItem* readingList =
      ReadingListActionItem();
  [mediator_ openMostVisitedItem:readingList atIndex:1];

  // Test.
  EXPECT_OCMOCK_VERIFY(dispatcher_);
}

// Tests that the command is sent to the loader when opening a most visited.
TEST_F(ContentSuggestionsMediatorTest, TestOpenMostVisited) {
  GURL url = GURL("http://chromium.org");
  ContentSuggestionsMostVisitedItem* item =
      [[ContentSuggestionsMostVisitedItem alloc] init];
  item.URL = url;
  OCMExpect([mediator_.NTPMetricsDelegate mostVisitedTileOpened]);

  // Action.
  [mediator_ openMostVisitedItem:item atIndex:0];

  // Test.
  EXPECT_EQ(url, url_loader_->last_params.web_params.url);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      url_loader_->last_params.web_params.transition_type));
}

TEST_F(ContentSuggestionsMediatorTest, TestOpenMostRecentTab) {
  // Create non-NTP WebState
  int recent_tab_index = web_state_list_->InsertWebState(
      0, CreateWebState("http://chromium.org"), WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
  favicon::WebFaviconDriver::CreateForWebState(
      web_state_list_->GetActiveWebState(),
      /*favicon_service=*/nullptr);
  StartSurfaceRecentTabBrowserAgent* browser_agent =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(browser_.get());
  browser_agent->SaveMostRecentTab();
  // Create NTP
  web_state_list_->InsertWebState(1, CreateWebState("chrome://newtab"),
                                  WebStateList::INSERT_ACTIVATE,
                                  WebStateOpener());
  web::WebState* ntp_web_state = web_state_list_->GetActiveWebState();
  mediator_.webState = ntp_web_state;
  NewTabPageTabHelper::FromWebState(ntp_web_state)->SetShowStartSurface(true);

  OCMExpect([consumer_ showReturnToRecentTabTileWithConfig:[OCMArg any]]);
  [mediator_
      configureMostRecentTabItemWithWebState:browser_agent->most_recent_tab()
                                   timeLabel:@"12 hours ago"];

  OCMExpect([consumer_ hideReturnToRecentTabTile]);
  OCMExpect([mediator_.NTPMetricsDelegate recentTabTileOpened]);

  [mediator_ openMostRecentTab];
  // Verify the most recent tab was opened.
  EXPECT_EQ(recent_tab_index, web_state_list_->active_index());
}

TEST_F(ContentSuggestionsMediatorTest, TestStartSurfaceRecentTabObserving) {
  // Create non-NTP WebState
  web_state_list_->InsertWebState(0, CreateWebState("http://chromium.org"),
                                  WebStateList::INSERT_ACTIVATE,
                                  WebStateOpener());
  favicon::WebFaviconDriver::CreateForWebState(
      web_state_list_->GetActiveWebState(),
      /*favicon_service=*/nullptr);
  StartSurfaceRecentTabBrowserAgent* browser_agent =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(browser_.get());
  browser_agent->SaveMostRecentTab();
  // Create NTP
  web_state_list_->InsertWebState(1, CreateWebState("chrome://newtab"),
                                  WebStateList::INSERT_ACTIVATE,
                                  WebStateOpener());
  web::WebState* web_state = browser_agent->most_recent_tab();
  [mediator_ configureMostRecentTabItemWithWebState:web_state
                                          timeLabel:@"12 hours ago"];

  OCMExpect([consumer_ updateReturnToRecentTabTileWithConfig:[OCMArg any]]);
  [mediator_ mostRecentTabFaviconUpdatedWithImage:[[UIImage alloc] init]];

  OCMExpect([consumer_ hideReturnToRecentTabTile]);
  [mediator_ mostRecentTabWasRemoved:web_state];
}

// Tests that the command is sent to the dispatcher when opening the What's new.
TEST_F(ContentSuggestionsMediatorTest, TestOpenWhatsNew) {
  OCMExpect([dispatcher_ showWhatsNew]);

  OCMExpect([mediator_.NTPMetricsDelegate shortcutTileOpened]);

  // Action.
  ContentSuggestionsMostVisitedActionItem* whatsNew = WhatsNewActionItem();
  [mediator_ openMostVisitedItem:whatsNew atIndex:1];
  // Test.
  EXPECT_OCMOCK_VERIFY(dispatcher_);
}

// Tests that the reload logic (e.g. setting the consumer) triggers the correct
// consumer calls when the Magic Stack feature is enabled.
TEST_F(ContentSuggestionsMediatorTest, TestMagicStackConsumerCall) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures({kMagicStack}, {});
  OCMExpect([consumer_ setMagicStackOrder:[OCMArg any]]);
  OCMExpect([consumer_ setShortcutTilesWithConfigs:[OCMArg any]]);
  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that when the user changes the setting to disable signin, the
// SetUpList signin item is marked complete.
TEST_F(ContentSuggestionsMediatorTest, TestOnServiceStatusChanged) {
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
