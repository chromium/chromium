// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mediator.h"

#import <memory>
#import <string_view>

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_audience.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/user_account_image_update_delegate.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_observer.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_factory.h"
#import "ios/chrome/browser/image_fetcher/model/image_fetcher_service_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_control_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/logo_vendor.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_consumer.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/toolbar/ui_bundled/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/test/fakes/fake_discover_feed_eligibility_handler.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/providers/discover_feed/test_discover_feed_service.h"
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
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_profile_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(test_profile_builder).Build();
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
    BrowserViewVisibilityNotifierBrowserAgent::CreateForBrowser(browser_.get());
    browser_view_visibility_notifier_ =
        BrowserViewVisibilityNotifierBrowserAgent::FromBrowser(browser_.get());
    // Set up discover feed.
    DiscoverFeedVisibilityBrowserAgent::CreateForBrowser(browser_.get());
    DiscoverFeedVisibilityBrowserAgent* discover_feed_visibility_browser_agent =
        DiscoverFeedVisibilityBrowserAgent::FromBrowser(browser_.get());
    discover_feed_visibility_browser_agent->SetEnabled(true);
    TestDiscoverFeedService* test_discover_feed_service =
        static_cast<TestDiscoverFeedService*>(
            DiscoverFeedServiceFactory::GetForProfile(profile_.get()));
    eligibility_handler_ =
        test_discover_feed_service->get_eligibility_handler();

    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_.get());
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    image_updater_ = OCMProtocolMock(@protocol(UserAccountImageUpdateDelegate));
    test_discover_feed_service_ = static_cast<TestDiscoverFeedService*>(
        DiscoverFeedServiceFactory::GetForProfile(profile_.get()));
    prefs_ = profile_->GetPrefs();
    HomeBackgroundCustomizationService* background_customization_service =
        HomeBackgroundCustomizationServiceFactory::GetForProfile(
            profile_.get());
    image_fetcher::ImageFetcherService* image_fetcher_service =
        ImageFetcherServiceFactory::GetForProfile(profile_.get());
    mediator_ = [[NewTabPageMediator alloc]
                initWithTemplateURLService:ios::TemplateURLServiceFactory::
                                               GetForProfile(profile_.get())
                                 URLLoader:url_loader_
                               authService:auth_service_
                           identityManager:identity_manager_
                     accountManagerService:account_manager_service
                  identityDiscImageUpdater:image_updater_
                       discoverFeedService:test_discover_feed_service_
                               prefService:prefs_
                               syncService:&test_sync_service_
               regionalCapabilitiesService:
                   ios::RegionalCapabilitiesServiceFactory::GetForProfile(
                       profile_.get())
            backgroundCustomizationService:background_customization_service
                       imageFetcherService:image_fetcher_service
             browserViewVisibilityNotifier:browser_view_visibility_notifier_
        discoverFeedVisibilityBrowserAgent:
            discover_feed_visibility_browser_agent];
    header_consumer_ = OCMProtocolMock(@protocol(NewTabPageHeaderConsumer));
    mediator_.headerConsumer = header_consumer_;
    visibility_observer_ =
        OCMProtocolMock(@protocol(DiscoverFeedVisibilityObserver));
    mediator_.feedVisibilityObserver = visibility_observer_;
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(profile_.get());
    feed_metrics_recorder_ =
        [[FeedMetricsRecorder alloc] initWithPrefService:prefs_
                                featureEngagementTracker:tracker];
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
  raw_ptr<PrefService> prefs_;
  id header_consumer_;
  id visibility_observer_;
  id image_updater_;
  id logo_vendor_;
  FeedMetricsRecorder* feed_metrics_recorder_;
  FakeDiscoverFeedEligibilityHandler* eligibility_handler_;
  NewTabPageMediator* mediator_;
  raw_ptr<ToolbarTestNavigationManager> navigation_manager_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  raw_ptr<BrowserViewVisibilityNotifierBrowserAgent>
      browser_view_visibility_notifier_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  syncer::TestSyncService test_sync_service_;
  raw_ptr<TestDiscoverFeedService> test_discover_feed_service_;
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
  EXPECT_EQ([eligibility_handler_ observerCount], 1);
}

// Tests that the feed visibility depends on the visibility of the discover
// visibility browser agent.
TEST_F(NewTabPageMediatorTest, TestShowAndHideFeed) {
  [mediator_ setUp];
  EXPECT_TRUE([mediator_ isFeedHeaderVisible]);
  [eligibility_handler_
      setEligibility:DiscoverFeedEligibility::kIneligibleReasonUnknown];
  EXPECT_FALSE([mediator_ isFeedHeaderVisible]);
  [eligibility_handler_ setEligibility:DiscoverFeedEligibility::kEligible];
  eligibility_handler_.enabled = false;
  EXPECT_FALSE([mediator_ isFeedHeaderVisible]);
}

// Tests that the mediator updates the Discover feed with the visibility state
// of the feed.
TEST_F(NewTabPageMediatorTest, TestUpdateVisibilityStateOfFeed) {
  using enum BrowserViewVisibilityState;

  [mediator_ setUp];

  UICollectionView* collection_view = [[UICollectionView alloc]
             initWithFrame:CGRectZero
      collectionViewLayout:[[UICollectionViewLayout alloc] init]];
  mediator_.contentCollectionView = collection_view;

  id<BrowserViewVisibilityAudience> audience =
      browser_view_visibility_notifier_->GetBrowserViewVisibilityAudience();

  // User is on new tab page.
  mediator_.NTPVisible = YES;
  [audience browserViewDidTransitionToVisibilityState:kAppearing
                                            fromState:kNotInViewHierarchy];
  EXPECT_EQ(test_discover_feed_service_->collection_view(), collection_view);
  EXPECT_EQ(test_discover_feed_service_->visibility_state(), kAppearing);

  // User turns off the feed.
  eligibility_handler_.enabled = false;
  [audience browserViewDidTransitionToVisibilityState:kVisible
                                            fromState:kAppearing];
  EXPECT_EQ(test_discover_feed_service_->visibility_state(), kAppearing);

  // User turns the feed back on.
  eligibility_handler_.enabled = true;
  [audience browserViewDidTransitionToVisibilityState:kCoveredByOmniboxPopup
                                            fromState:kVisible];
  EXPECT_EQ(test_discover_feed_service_->visibility_state(),
            kCoveredByOmniboxPopup);

  // User has navigated away.
  mediator_.NTPVisible = NO;
  [audience browserViewDidTransitionToVisibilityState:kVisible
                                            fromState:kCoveredByOmniboxPopup];
  EXPECT_EQ(test_discover_feed_service_->visibility_state(),
            kCoveredByOmniboxPopup);
}
