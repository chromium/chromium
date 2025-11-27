// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mediator.h"

#import <memory>
#import <string_view>

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
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
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager_factory.h"
#import "ios/chrome/browser/image_fetcher/model/image_fetcher_service_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_control_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_consumer.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"
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

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
    BrowserViewVisibilityNotifierBrowserAgent::CreateForBrowser(browser_.get());
    browser_view_visibility_notifier_ =
        BrowserViewVisibilityNotifierBrowserAgent::FromBrowser(browser_.get());
    // Set up discover feed.
    DiscoverFeedVisibilityBrowserAgent::CreateForBrowser(browser_.get());
    discover_feed_visibility_browser_agent_ =
        DiscoverFeedVisibilityBrowserAgent::FromBrowser(browser_.get());
    discover_feed_visibility_browser_agent_->SetEnabled(true);
    TestDiscoverFeedService* test_discover_feed_service =
        static_cast<TestDiscoverFeedService*>(
            DiscoverFeedServiceFactory::GetForProfile(profile_.get()));
    eligibility_handler_ =
        test_discover_feed_service->get_eligibility_handler();

    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_.get());
    image_updater_ = OCMProtocolMock(@protocol(UserAccountImageUpdateDelegate));
    test_discover_feed_service_ = static_cast<TestDiscoverFeedService*>(
        DiscoverFeedServiceFactory::GetForProfile(profile_.get()));
    prefs_ = profile_->GetPrefs();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    aim_eligibility_service_ =
        std::make_unique<testing::StrictMock<MockAimEligibilityService>>(
            *profile_->GetPrefs(),
            ios::TemplateURLServiceFactory::GetForProfile(profile_.get()),
            nullptr, identity_manager_);
  }

  /// Creates mediator with optional `aim_eligibility_service`.
  void CreateMediator(bool with_aim_eligibility_service = false) {
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    HomeBackgroundCustomizationService* background_customization_service =
        HomeBackgroundCustomizationServiceFactory::GetForProfile(
            profile_.get());
    image_fetcher::ImageFetcherService* image_fetcher_service =
        ImageFetcherServiceFactory::GetForProfile(profile_.get());
    UserUploadedImageManager* user_uploaded_image_manager =
        UserUploadedImageManagerFactory::GetForProfile(profile_.get());

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
                  userUploadedImageManager:user_uploaded_image_manager
             browserViewVisibilityNotifier:browser_view_visibility_notifier_
        discoverFeedVisibilityBrowserAgent:
            discover_feed_visibility_browser_agent_
                  featureEngagementTracker:&mock_tracker_
                     aimEligibilityService:with_aim_eligibility_service
                                               ? aim_eligibility_service_.get()
                                               : nullptr];
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

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<web::WebState> initial_web_state_;
  feature_engagement::test::MockTracker mock_tracker_;
  raw_ptr<PrefService> prefs_;
  id header_consumer_;
  id visibility_observer_;
  id image_updater_;
  FeedMetricsRecorder* feed_metrics_recorder_;
  raw_ptr<DiscoverFeedVisibilityBrowserAgent>
      discover_feed_visibility_browser_agent_;
  FakeDiscoverFeedEligibilityHandler* eligibility_handler_;
  NewTabPageMediator* mediator_;
  raw_ptr<ToolbarTestNavigationManager, DanglingUntriaged> navigation_manager_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  raw_ptr<BrowserViewVisibilityNotifierBrowserAgent>
      browser_view_visibility_notifier_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  syncer::TestSyncService test_sync_service_;
  raw_ptr<TestDiscoverFeedService> test_discover_feed_service_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockAimEligibilityService> aim_eligibility_service_;
};

// Tests that the consumer has the right value set up.
TEST_F(NewTabPageMediatorTest, TestConsumerSetup) {
  // Setup.
  CreateMediator();
  OCMExpect(
      [header_consumer_ setSearchEngineLogoState:SearchEngineLogoState::kLogo]);

  // Action.
  [mediator_ setUp];

  // Tests.
  EXPECT_OCMOCK_VERIFY(header_consumer_);
  EXPECT_EQ([eligibility_handler_ observerCount], 1);
}

// Tests that the feed visibility depends on the visibility of the discover
// visibility browser agent.
TEST_F(NewTabPageMediatorTest, TestShowAndHideFeed) {
  CreateMediator();
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

  CreateMediator();
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

// Tests that -notifyLensBadgeDisplayed correctly notifies the tracker.
TEST_F(NewTabPageMediatorTest, TestNotifyLensBadgeDisplayed) {
  CreateMediator();
  EXPECT_CALL(
      mock_tracker_,
      Dismissed(testing::Ref(feature_engagement::kIPHiOSHomepageLensNewBadge)));
  [mediator_ notifyLensBadgeDisplayed];
}

// Tests that -notifyCustomizationBadgeDisplayed correctly notifies the tracker.
TEST_F(NewTabPageMediatorTest, TestNotifyCustomizationBadgeDisplayed) {
  CreateMediator();
  EXPECT_CALL(mock_tracker_,
              Dismissed(testing::Ref(
                  feature_engagement::kIPHiOSHomepageCustomizationNewBadge)));
  [mediator_ notifyCustomizationBadgeDisplayed];
}

// Tests that -checkNewBadgeEligibility notifies the feature engagement tracker
// only when the first run was not recent.
TEST_F(NewTabPageMediatorTest, TestCheckNewBadgeEligibilityNotifiesTracker) {
  CreateMediator();
  // First Run is 1 day old, so the tracker should be notified.
  ForceFirstRunRecency(1);
  EXPECT_CALL(
      mock_tracker_,
      NotifyEvent(
          feature_engagement::events::kIOSFREBadgeHoldbackPeriodElapsed));
  [mediator_ checkNewBadgeEligibility];

  ResetFirstRunSentinel();
}

// Tests that -checkNewBadgeEligibility does not notify the feature engagement
// tracker if it is the First Run.
TEST_F(NewTabPageMediatorTest,
       TestCheckNewBadgeEligibilityDoesNotNotifyTrackerOnFirstRun) {
  CreateMediator();
  // It is the First Run, so the tracker should not be notified.
  ResetFirstRunSentinel();
  EXPECT_CALL(
      mock_tracker_,
      NotifyEvent(
          feature_engagement::events::kIOSFREBadgeHoldbackPeriodElapsed))
      .Times(0);
  [mediator_ checkNewBadgeEligibility];
}

// Tests that the AIM is disabled if the user is not eligible.
TEST_F(NewTabPageMediatorTest, TestAIMNotEligible) {
  scoped_feature_list_.InitAndEnableFeature(kAIMNTPEntrypointTablet);
  // Setup with non eligible.
  EXPECT_CALL(*aim_eligibility_service_,
              RegisterEligibilityChangedCallback(testing::_))
      .WillOnce(testing::Return(base::CallbackListSubscription()));
  CreateMediator(/*with_aim_eligibility_service=*/true);
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(false));
  OCMExpect(
      [header_consumer_ setSearchEngineLogoState:SearchEngineLogoState::kLogo]);

  // Consumer should be notified.
  id ntp_consumer = OCMProtocolMock(@protocol(NewTabPageConsumer));
  mediator_.consumer = ntp_consumer;
  OCMExpect([ntp_consumer setAIMAllowed:NO]);
  OCMExpect([header_consumer_ setAIMAllowed:NO]);
  [mediator_ setUp];

  EXPECT_OCMOCK_VERIFY(header_consumer_);
  EXPECT_OCMOCK_VERIFY(ntp_consumer);
}

// Tests that the AIM is enabled if the user is eligible.
TEST_F(NewTabPageMediatorTest, TestAIMEligible) {
  scoped_feature_list_.InitAndEnableFeature(kAIMNTPEntrypointTablet);
  // Setup with eligible.
  EXPECT_CALL(*aim_eligibility_service_,
              RegisterEligibilityChangedCallback(testing::_))
      .WillOnce(testing::Return(base::CallbackListSubscription()));
  CreateMediator(/*with_aim_eligibility_service=*/true);
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible())
      .WillRepeatedly(testing::Return(true));
  OCMExpect(
      [header_consumer_ setSearchEngineLogoState:SearchEngineLogoState::kLogo]);

  // Consumer should be notified.
  id ntp_consumer = OCMProtocolMock(@protocol(NewTabPageConsumer));
  mediator_.consumer = ntp_consumer;
  OCMExpect([ntp_consumer setAIMAllowed:YES]);
  OCMExpect([header_consumer_ setAIMAllowed:YES]);
  [mediator_ setUp];

  EXPECT_OCMOCK_VERIFY(header_consumer_);
  EXPECT_OCMOCK_VERIFY(ntp_consumer);
}

// Tests that NTP modules are not updated if AIM eligibility changes before
// setup.
TEST_F(NewTabPageMediatorTest, TestAIMBecomeEligibleBeforeSetUp) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_featured=*/{kAIMEligibilityRefreshNTPModules,
                            kAIMNTPEntrypointTablet},
      /*disabled_features=*/{});
  // Setup with non eligible.
  base::RepeatingClosure callback;
  EXPECT_CALL(*aim_eligibility_service_,
              RegisterEligibilityChangedCallback(testing::_))
      .WillOnce([&](base::RepeatingClosure c) {
        callback = c;
        return base::CallbackListSubscription();
      });

  bool is_eligible = false;
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible()).WillRepeatedly([&]() {
    return is_eligible;
  });

  CreateMediator(/*with_aim_eligibility_service=*/true);

  id ntp_consumer = OCMProtocolMock(@protocol(NewTabPageConsumer));
  mediator_.consumer = ntp_consumer;
  id ntp_content_delegate =
      OCMProtocolMock(@protocol(NewTabPageContentDelegate));
  mediator_.NTPContentDelegate = ntp_content_delegate;

  // Eligibility changes to true before setup.
  is_eligible = true;
  OCMReject([ntp_content_delegate updateModuleVisibility]);
  OCMExpect([ntp_consumer setAIMAllowed:YES]);
  OCMExpect([header_consumer_ setAIMAllowed:YES]);
  callback.Run();
  // Consumer are updated but modules are not.
  EXPECT_OCMOCK_VERIFY(ntp_content_delegate);

  // Consumer are updated during setup.
  OCMExpect(
      [header_consumer_ setSearchEngineLogoState:SearchEngineLogoState::kLogo]);
  OCMExpect([ntp_consumer setAIMAllowed:YES]);
  OCMExpect([header_consumer_ setAIMAllowed:YES]);
  [mediator_ setUp];

  EXPECT_OCMOCK_VERIFY(header_consumer_);
  EXPECT_OCMOCK_VERIFY(ntp_consumer);
}

// Tests that NTP modules are updated if AIM eligibility changes after setup.
TEST_F(NewTabPageMediatorTest, TestAIMBecomeEligibleAfterSetUp) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_featured=*/{kAIMEligibilityRefreshNTPModules,
                            kAIMNTPEntrypointTablet},
      /*disabled_features=*/{});
  // Setup with non eligible.
  base::RepeatingClosure callback;
  EXPECT_CALL(*aim_eligibility_service_,
              RegisterEligibilityChangedCallback(testing::_))
      .WillOnce([&](base::RepeatingClosure c) {
        callback = c;
        return base::CallbackListSubscription();
      });

  bool is_eligible = false;
  EXPECT_CALL(*aim_eligibility_service_, IsAimEligible()).WillRepeatedly([&]() {
    return is_eligible;
  });

  CreateMediator(/*with_aim_eligibility_service=*/true);
  OCMExpect(
      [header_consumer_ setSearchEngineLogoState:SearchEngineLogoState::kLogo]);

  // Setup with non eligible.
  id ntp_consumer = OCMProtocolMock(@protocol(NewTabPageConsumer));
  mediator_.consumer = ntp_consumer;
  id ntp_content_delegate =
      OCMProtocolMock(@protocol(NewTabPageContentDelegate));
  mediator_.NTPContentDelegate = ntp_content_delegate;
  [mediator_ setUp];

  // Becomes eligible after setup, modules should be updated.
  is_eligible = true;
  OCMExpect([ntp_consumer setAIMAllowed:YES]);
  OCMExpect([header_consumer_ setAIMAllowed:YES]);
  OCMExpect([ntp_content_delegate updateModuleVisibility]);
  callback.Run();

  EXPECT_OCMOCK_VERIFY(header_consumer_);
  EXPECT_OCMOCK_VERIFY(ntp_consumer);
  EXPECT_OCMOCK_VERIFY(ntp_content_delegate);
}
