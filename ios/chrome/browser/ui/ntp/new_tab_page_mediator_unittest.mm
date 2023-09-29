// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_mediator.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/ntp/logo_vendor.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_consumer.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using feed::FeedUserActionType;

// Expects a URL to start with a prefix.
#define EXPECT_URL_PREFIX(url, prefix) \
  EXPECT_STREQ(url.spec().substr(0, strlen(prefix)).c_str(), prefix);

// Expects the URL loader to have loaded a URL that has the given `prefix`.
#define EXPECT_URL_LOAD(prefix) \
  EXPECT_URL_PREFIX(url_loader_->last_params.web_params.url, prefix);

class NewTabPageMediatorTest : public PlatformTest {
 public:
  NewTabPageMediatorTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    std::unique_ptr<ToolbarTestNavigationManager> navigation_manager =
        std::make_unique<ToolbarTestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    initial_web_state_ = CreateWebStateWithURL(GURL("chrome://newtab"), 0.0);
    logo_vendor_ = OCMProtocolMock(@protocol(LogoVendor));

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get()));
    identity_manager_ =
        IdentityManagerFactory::GetForBrowserState(chrome_browser_state_.get());
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    image_updater_ = OCMProtocolMock(@protocol(UserAccountImageUpdateDelegate));
    bool is_incognito = chrome_browser_state_.get()->IsOffTheRecord();
    DiscoverFeedService* discover_feed_service =
        DiscoverFeedServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    mediator_ = [[NewTabPageMediator alloc]
        initWithTemplateURLService:ios::TemplateURLServiceFactory::
                                       GetForBrowserState(
                                           chrome_browser_state_.get())
                         URLLoader:url_loader_
                       authService:auth_service_
                   identityManager:identity_manager_
             accountManagerService:account_manager_service
          identityDiscImageUpdater:image_updater_
                       isIncognito:is_incognito
               discoverFeedService:discover_feed_service];
    header_consumer_ = OCMProtocolMock(@protocol(NewTabPageHeaderConsumer));
    mediator_.headerConsumer = header_consumer_;
    PrefService* prefs = chrome_browser_state_->GetPrefs();
    feed_metrics_recorder_ =
        [[FeedMetricsRecorder alloc] initWithPrefService:prefs];
    mediator_.feedMetricsRecorder = feed_metrics_recorder_;
    histogram_tester_.reset(new base::HistogramTester());
  }

  // Explicitly disconnect the mediator.
  ~NewTabPageMediatorTest() override { [mediator_ shutdown]; }

  // Creates a FakeWebState and simulates that it is loaded with a given `url`.
  std::unique_ptr<web::WebState> CreateWebStateWithURL(
      const GURL& url,
      CGFloat scroll_position = 0.0) {
    auto web_state = std::make_unique<web::FakeWebState>();
    NewTabPageTabHelper::CreateForWebState(web_state.get());
    web_state->SetVisibleURL(url);
    // Force the DidStopLoading callback.
    web_state->SetLoading(true);
    web_state->SetLoading(false);
    return std::move(web_state);
  }

  id SetupNTPConsumerMock() {
    id ntp_consumer = OCMProtocolMock(@protocol(NewTabPageConsumer));
    [[[ntp_consumer expect] andReturnValue:[NSNumber numberWithDouble:0.0]]
        heightAboveFeed];
    mediator_.consumer = ntp_consumer;
    return ntp_consumer;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<web::WebState> initial_web_state_;
  id header_consumer_;
  id image_updater_;
  id logo_vendor_;
  FeedMetricsRecorder* feed_metrics_recorder_;
  NewTabPageMediator* mediator_;
  ToolbarTestNavigationManager* navigation_manager_;
  FakeUrlLoadingBrowserAgent* url_loader_;
  AuthenticationService* auth_service_;
  signin::IdentityManager* identity_manager_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests that the consumer has the right value set up.
TEST_F(NewTabPageMediatorTest, TestConsumerSetup) {
  // Setup.
  OCMExpect([header_consumer_ setLogoIsShowing:YES]);

  // Action.
  [mediator_ setUp];

  // Tests.
  EXPECT_OCMOCK_VERIFY(header_consumer_);
}

// Tests that the FeedManagementNavigationDelegate methods load URLs and
// record metrics.
TEST_F(NewTabPageMediatorTest, TestFeedManagementNavigationDelegate) {
  [mediator_ handleNavigateToActivity];
  EXPECT_URL_LOAD("https://myactivity.google.com/myactivity");
  histogram_tester_->ExpectUniqueSample(
      kDiscoverFeedUserActionHistogram,
      FeedUserActionType::kTappedManageActivity, 1);

  histogram_tester_.reset(new base::HistogramTester());
  [mediator_ handleNavigateToInterests];
  EXPECT_URL_LOAD("https://google.com/preferences/interests");
  histogram_tester_->ExpectUniqueSample(
      kDiscoverFeedUserActionHistogram,
      FeedUserActionType::kTappedManageInterests, 1);

  histogram_tester_.reset(new base::HistogramTester());
  [mediator_ handleNavigateToHidden];
  EXPECT_URL_LOAD("https://google.com/preferences/interests/hidden");
  histogram_tester_->ExpectUniqueSample(kDiscoverFeedUserActionHistogram,
                                        FeedUserActionType::kTappedManageHidden,
                                        1);

  histogram_tester_.reset(new base::HistogramTester());
  GURL followed_url("https://example.org");
  [mediator_ handleNavigateToFollowedURL:followed_url];
  EXPECT_URL_LOAD(followed_url.spec().c_str());
  // TODO(crbug.com/1331102): Add metrics.
}

// Tests that the handleFeedLearnMoreTapped loads the correct URL and records
// metrics.
TEST_F(NewTabPageMediatorTest, TestHandleFeedLearnMoreTapped) {
  [mediator_ handleFeedLearnMoreTapped];
  EXPECT_URL_LOAD("https://support.google.com/chrome/"
                  "?p=new_tab&co=GENIE.Platform%3DiOS&oco=1");
  histogram_tester_->ExpectUniqueSample(kDiscoverFeedUserActionHistogram,
                                        FeedUserActionType::kTappedLearnMore,
                                        1);
}
