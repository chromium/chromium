// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_timeouts.h"
#import "base/time/default_clock.h"
#import "base/time/time.h"
#import "components/favicon/core/test/mock_favicon_service.h"
#import "components/ntp_tiles/icon_cacher.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Testing Suite for ContentSuggestionsMediator
class ContentSuggestionsMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    web_state_list_ = browser_->GetWebStateList();
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    fake_web_state_->SetBrowserState(chrome_browser_state_.get());
    NewTabPageTabHelper::CreateForWebState(fake_web_state_.get());
    consumer_ = OCMProtocolMock(@protocol(ContentSuggestionsConsumer));

    SetUpMediator();
    mediator_.consumer = consumer_;

    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  ~ContentSuggestionsMediatorTest() override { [mediator_ disconnect]; }

 protected:
  void SetUpMediator() {
    mediator_ =
        [[ContentSuggestionsMediator alloc] initWithBrowser:browser_.get()];
    mediator_.NTPMetricsDelegate =
        OCMProtocolMock(@protocol(NewTabPageMetricsDelegate));
  }

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
  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  id consumer_;
  ContentSuggestionsMediator* mediator_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  ContentSuggestionsMetricsRecorder* metrics_recorder_;
};

// Tests that MostRecentTab can be opened and that its title is correct when
// tab has a title.
TEST_F(ContentSuggestionsMediatorTest, TestOpenMostRecentTab) {
  // Create non-NTP WebState
  auto web_state = CreateWebState("http://chromium.org");
  web_state->SetTitle(u"title");
  int recent_tab_index = web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());
  favicon::WebFaviconDriver::CreateForWebState(
      web_state_list_->GetActiveWebState(),
      /*favicon_service=*/nullptr);
  StartSurfaceRecentTabBrowserAgent* browser_agent =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(browser_.get());
  browser_agent->SaveMostRecentTab();
  // Create NTP
  web_state_list_->InsertWebState(
      CreateWebState("chrome://newtab"),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* ntp_web_state = web_state_list_->GetActiveWebState();
  NewTabPageTabHelper::FromWebState(ntp_web_state)->SetShowStartSurface(true);

  OCMExpect([consumer_
      updateReturnToRecentTabTileWithConfig:
          [OCMArg
              checkWithBlock:^(ContentSuggestionsReturnToRecentTabItem* item) {
                EXPECT_NSEQ(@"title - 12 hours ago", item.subtitle);
                return YES;
              }]]);
  [mediator_
      configureMostRecentTabItemWithWebState:browser_agent->most_recent_tab()
                                   timeLabel:@"12 hours ago"];

  OCMExpect([consumer_ hideReturnToRecentTabTile]);
  OCMExpect([mediator_.NTPMetricsDelegate recentTabTileOpened]);

  [mediator_ openMostRecentTab];
  // Verify the most recent tab was opened.
  EXPECT_EQ(recent_tab_index, web_state_list_->active_index());
}

// Tests that MostRecentTab can be opened and that its title is correct when
// tab has a no title.
TEST_F(ContentSuggestionsMediatorTest, TestOpenMostRecentTabNoTitle) {
  // Create non-NTP WebState
  int recent_tab_index = web_state_list_->InsertWebState(
      CreateWebState("http://chromium.org"),
      WebStateList::InsertionParams::Automatic().Activate());
  favicon::WebFaviconDriver::CreateForWebState(
      web_state_list_->GetActiveWebState(),
      /*favicon_service=*/nullptr);
  StartSurfaceRecentTabBrowserAgent* browser_agent =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(browser_.get());
  browser_agent->SaveMostRecentTab();
  // Create NTP
  web_state_list_->InsertWebState(
      CreateWebState("chrome://newtab"),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* ntp_web_state = web_state_list_->GetActiveWebState();
  NewTabPageTabHelper::FromWebState(ntp_web_state)->SetShowStartSurface(true);

  OCMExpect([consumer_
      updateReturnToRecentTabTileWithConfig:
          [OCMArg
              checkWithBlock:^(ContentSuggestionsReturnToRecentTabItem* item) {
                EXPECT_NSEQ(@"chromium.org - 12 hours ago", item.subtitle);
                return YES;
              }]]);
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
  web_state_list_->InsertWebState(
      CreateWebState("http://chromium.org"),
      WebStateList::InsertionParams::Automatic().Activate());
  favicon::WebFaviconDriver::CreateForWebState(
      web_state_list_->GetActiveWebState(),
      /*favicon_service=*/nullptr);
  StartSurfaceRecentTabBrowserAgent* browser_agent =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(browser_.get());
  browser_agent->SaveMostRecentTab();
  // Create NTP
  web_state_list_->InsertWebState(
      CreateWebState("chrome://newtab"),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* web_state = browser_agent->most_recent_tab();
  [mediator_ configureMostRecentTabItemWithWebState:web_state
                                          timeLabel:@" - 12 hours ago"];

  OCMExpect([consumer_ updateReturnToRecentTabTileWithConfig:[OCMArg any]]);
  [mediator_ mostRecentTab:web_state
      faviconUpdatedWithImage:[[UIImage alloc] init]];

  OCMExpect([consumer_ hideReturnToRecentTabTile]);
  [mediator_ mostRecentTabWasRemoved:web_state];
}
