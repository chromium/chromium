// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mediator.h"

#import <memory>
#import <string_view>

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_control_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/logo_vendor.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_consumer.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
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
    TestProfileIOS::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(test_cbs_builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(profile_.get());

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
        AuthenticationServiceFactory::GetInstance()->GetForProfile(
            profile_.get()));
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_.get());
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    image_updater_ = OCMProtocolMock(@protocol(UserAccountImageUpdateDelegate));
    bool is_incognito = profile_.get()->IsOffTheRecord();
    DiscoverFeedService* discover_feed_service =
        DiscoverFeedServiceFactory::GetForProfile(profile_.get());
    PrefService* prefs = profile_->GetPrefs();
    mediator_ = [[NewTabPageMediator alloc]
        initWithTemplateURLService:ios::TemplateURLServiceFactory::
                                       GetForProfile(profile_.get())
                         URLLoader:url_loader_
                       authService:auth_service_
                   identityManager:identity_manager_
             accountManagerService:account_manager_service
          identityDiscImageUpdater:image_updater_
                       isIncognito:is_incognito
               discoverFeedService:discover_feed_service
                       prefService:prefs
                       syncService:&test_sync_service_
                        isSafeMode:NO];
    header_consumer_ = OCMProtocolMock(@protocol(NewTabPageHeaderConsumer));
    mediator_.headerConsumer = header_consumer_;
    feed_metrics_recorder_ =
        [[FeedMetricsRecorder alloc] initWithPrefService:prefs];
    mediator_.feedMetricsRecorder = feed_metrics_recorder_;
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Explicitly disconnect the mediator.
  ~NewTabPageMediatorTest() override { [mediator_ shutdown]; }

  // Creates a FakeWebState and simulates that it is loaded with a given `url`.
  std::unique_ptr<web::WebState> CreateWebStateWithURL(
      const GURL& url,
      CGFloat scroll_position = 0.0) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
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

  void SetCustomSearchEngine() {
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    // A custom search engine will have a `prepopulate_id` of 0.
    const int kCustomSearchEnginePrepopulateId = 0;
    TemplateURLData template_url_data;
    template_url_data.prepopulate_id = kCustomSearchEnginePrepopulateId;
    template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
    template_url_service->SetUserSelectedDefaultSearchProvider(
        template_url_service->Add(
            std::make_unique<TemplateURL>(template_url_data)));
  }

  void OverrideSearchEngineChoiceCountry(std::string_view country) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, country);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<web::WebState> initial_web_state_;
  id header_consumer_;
  id image_updater_;
  id logo_vendor_;
  FeedMetricsRecorder* feed_metrics_recorder_;
  NewTabPageMediator* mediator_;
  raw_ptr<ToolbarTestNavigationManager> navigation_manager_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  syncer::TestSyncService test_sync_service_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
  [mediator_ handleNavigateToFollowing];
  EXPECT_URL_LOAD("https://google.com/preferences/interests");
  histogram_tester_->ExpectUniqueSample(
      kDiscoverFeedUserActionHistogram,
      FeedUserActionType::kTappedManageFollowing, 1);

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
  // TODO(crbug.com/40227407): Add metrics.
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

// Tests that the feed will be hidden when a non-Google search engine is chosen,
// but only in EEA countries.
TEST_F(NewTabPageMediatorTest, TestHideFeedWithSearchChoiceTargeted) {
  // Test it with the default search engine, with country set to France.
  OverrideSearchEngineChoiceCountry("FR");
  [mediator_ setUp];
  EXPECT_TRUE(mediator_.feedHeaderVisible);

  // Set up expectation for custom search engine, country set to France.
  id feed_control_delegate = OCMProtocolMock(@protocol(FeedControlDelegate));
  OCMExpect([feed_control_delegate setFeedAndHeaderVisibility:NO]);
  mediator_.feedControlDelegate = feed_control_delegate;

  // Test setting a custom search engine, country still set to France.
  SetCustomSearchEngine();
  EXPECT_FALSE(mediator_.feedHeaderVisible);
  EXPECT_OCMOCK_VERIFY(feed_control_delegate);

  // Set up expectation for custom search engine, with country set to US.
  feed_control_delegate = OCMProtocolMock(@protocol(FeedControlDelegate));
  OCMExpect([feed_control_delegate setFeedAndHeaderVisibility:YES]);
  mediator_.feedControlDelegate = feed_control_delegate;

  // Test with custom search engine, with country set to US.
  OverrideSearchEngineChoiceCountry("US");
  SetCustomSearchEngine();
  EXPECT_TRUE(mediator_.feedHeaderVisible);
  EXPECT_OCMOCK_VERIFY(feed_control_delegate);
}
