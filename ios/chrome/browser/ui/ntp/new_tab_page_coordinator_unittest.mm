// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/service/variations_service_client.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"
#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_constants.h"
#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_component_factory.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator+private.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using variations::UIStringOverrider;
using variations::VariationsService;
using variations::VariationsServiceClient;

namespace {

// TODO(crbug.com/1167566): Remove when fake VariationsServiceClient created.
class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() = default;
  TestVariationsServiceClient(const TestVariationsServiceClient&) = delete;
  TestVariationsServiceClient& operator=(const TestVariationsServiceClient&) =
      delete;
  ~TestVariationsServiceClient() override = default;

  // VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    return false;
  }
  bool IsEnterprise() override { return false; }
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}

 private:
  // VariationsServiceClient:
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
};

// Creates a VariationsService and sets it as the TestingApplicationContext's
// VariationService for the life of the instance.
class ScopedVariationsService {
 public:
  ScopedVariationsService() {
    EXPECT_EQ(nullptr,
              TestingApplicationContext::GetGlobal()->GetVariationsService());
    variations_service_ = VariationsService::Create(
        std::make_unique<TestVariationsServiceClient>(),
        TestingApplicationContext::GetGlobal()->GetLocalState(),
        /*state_manager=*/nullptr, "dummy-disable-background-switch",
        UIStringOverrider(),
        network::TestNetworkConnectionTracker::CreateGetter());
    TestingApplicationContext::GetGlobal()->SetVariationsService(
        variations_service_.get());
  }

  ~ScopedVariationsService() {
    EXPECT_EQ(variations_service_.get(),
              TestingApplicationContext::GetGlobal()->GetVariationsService());
    TestingApplicationContext::GetGlobal()->SetVariationsService(nullptr);
    variations_service_.reset();
  }

  VariationsService* Get() { return variations_service_.get(); }

  std::unique_ptr<VariationsService> variations_service_;
};

}  // namespace

// Test fixture for testing NewTabPageCoordinator class.
class NewTabPageCoordinatorTest : public PlatformTest {
 protected:
  NewTabPageCoordinatorTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())),
        base_view_controller_([[UIViewController alloc] init]) {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = test_cbs_builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    toolbar_delegate_ =
        OCMProtocolMock(@protocol(NewTabPageControllerDelegate));
    histogram_tester_.reset(new base::HistogramTester());

    std::vector<base::test::FeatureRef> enabled;
    enabled.push_back(kEnableDiscoverFeedTopSyncPromo);
    std::vector<base::test::FeatureRef> disabled;
    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  std::unique_ptr<web::FakeWebState> CreateWebState(const char* url) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    NewTabPageTabHelper::CreateForWebState(test_web_state.get());
    test_web_state->SetCurrentURL(GURL(url));
    test_web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    test_web_state->SetBrowserState(browser_state_.get());
    return test_web_state;
  }

  void CreateCoordinator(bool off_the_record) {
    if (off_the_record) {
      ChromeBrowserState* otr_state =
          browser_state_->GetOffTheRecordChromeBrowserState();
      browser_ = std::make_unique<TestBrowser>(otr_state);
    } else {
      browser_ = std::make_unique<TestBrowser>(browser_state_.get());
      StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());
      // Create non-NTP WebState
      browser_.get()->GetWebStateList()->InsertWebState(
          0, CreateWebState("http://chromium.org"),
          WebStateList::INSERT_ACTIVATE, WebStateOpener());
      favicon::WebFaviconDriver::CreateForWebState(
          browser_.get()->GetWebStateList()->GetActiveWebState(),
          /*favicon_service=*/nullptr);
      StartSurfaceRecentTabBrowserAgent* browser_agent =
          StartSurfaceRecentTabBrowserAgent::FromBrowser(browser_.get());
      browser_agent->SaveMostRecentTab();
    }
    scene_state_ = OCMClassMock([SceneState class]);
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

    // Mocks the component factory so that the NTP is tested with a fake feed.
    // This allows testing of feed-dependent views, such as the feed top
    // section.
    // TODO(crbug.com/1441139): Replace this with a
    // FakeNewTabPageComponentFactory implementation.
    component_factory_mock_ =
        OCMPartialMock([[NewTabPageComponentFactory alloc] init]);
    fake_feed_view_controller_ = [[UIViewController alloc] init];
    UICollectionView* fakeFeedCollectionView = [[UICollectionView alloc]
               initWithFrame:CGRectZero
        collectionViewLayout:[[UICollectionViewFlowLayout alloc] init]];
    fakeFeedCollectionView.translatesAutoresizingMaskIntoConstraints = NO;
    [fake_feed_view_controller_.view addSubview:fakeFeedCollectionView];
    FeedWrapperViewController* feedWrapperViewController =
        [[FeedWrapperViewController alloc]
              initWithDelegate:coordinator_
            feedViewController:fake_feed_view_controller_];
    OCMExpect([component_factory_mock_ discoverFeedForBrowser:browser_.get()
                                  viewControllerConfiguration:[OCMArg any]])
        .andReturn(fake_feed_view_controller_);
    OCMStub([component_factory_mock_
                feedWrapperViewControllerWithDelegate:[OCMArg any]
                                   feedViewController:[OCMArg any]])
        .andReturn(feedWrapperViewController);

    coordinator_ =
        [[NewTabPageCoordinator alloc] initWithBrowser:browser_.get()
                                      componentFactory:component_factory_mock_];
    coordinator_.baseViewController = base_view_controller_;
    coordinator_.toolbarDelegate = toolbar_delegate_;

    NTPMetricsRecorder_ = [[NewTabPageMetricsRecorder alloc] init];
    coordinator_.NTPMetricsRecorder = NTPMetricsRecorder_;

    InsertWebState(CreateWebStateWithURL(GURL("chrome://newtab")));
  }

  // Inserts a FakeWebState into the browser's WebStateList.
  void InsertWebState(std::unique_ptr<web::WebState> web_state) {
    browser_->GetWebStateList()->InsertWebState(
        /*index=*/0, std::move(web_state), WebStateList::INSERT_ACTIVATE,
        WebStateOpener());
    web_state_ = browser_->GetWebStateList()->GetActiveWebState();
  }

  // Creates a FakeWebState and simulates that it is loaded with a given `url`.
  std::unique_ptr<web::WebState> CreateWebStateWithURL(const GURL& url) {
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    NewTabPageTabHelper::CreateForWebState(web_state.get());
    web_state->SetVisibleURL(url);
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    web_state->SetNavigationManager(std::move(navigation_manager));

    // Force the URL load callbacks.
    web::FakeNavigationContext navigation_context;
    web_state->OnNavigationStarted(&navigation_context);
    web_state->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
    return std::move(web_state);
  }

  // Simulates loading the NTP in `web_state_`.
  void SetNTPAsCurrentURL() {
    web::FakeNavigationContext navigation_context;
    navigation_context.SetUrl(GURL("chrome://newtab"));
    web::FakeWebState* fake_web_state =
        static_cast<web::FakeWebState*>(web_state_);
    fake_web_state->SetVisibleURL(GURL("chrome://newtab"));
    fake_web_state->OnNavigationStarted(&navigation_context);
    fake_web_state->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  }

  void SetupCommandHandlerMocks() {
    omnibox_commands_handler_mock = OCMProtocolMock(@protocol(OmniboxCommands));
    snackbar_commands_handler_mock =
        OCMProtocolMock(@protocol(SnackbarCommands));
    fakebox_focuser_handler_mock = OCMProtocolMock(@protocol(FakeboxFocuser));
    [browser_.get()->GetCommandDispatcher()
        startDispatchingToTarget:omnibox_commands_handler_mock
                     forProtocol:@protocol(OmniboxCommands)];
    [browser_.get()->GetCommandDispatcher()
        startDispatchingToTarget:snackbar_commands_handler_mock
                     forProtocol:@protocol(SnackbarCommands)];
    [browser_.get()->GetCommandDispatcher()
        startDispatchingToTarget:fakebox_focuser_handler_mock
                     forProtocol:@protocol(FakeboxFocuser)];
  }

  // Dynamically calls a selector on an object.
  void DynamicallyCallSelector(id object, SEL selector, Class klass) {
    NSMethodSignature* signature =
        [klass instanceMethodSignatureForSelector:selector];
    // Note: numberOfArguments is always at least 2 (self and _cmd).
    ASSERT_EQ(int(signature.numberOfArguments), 2);
    NSInvocation* invocation =
        [NSInvocation invocationWithMethodSignature:signature];
    invocation.selector = selector;
    [invocation invokeWithTarget:object];
  }

  // Expects a coordinator method call to call a view controller method.
  void ExpectMethodToProxyToVC(SEL coordinator_selector,
                               SEL view_controller_selector) {
    NewTabPageViewController* original_vc = coordinator_.NTPViewController;
    id view_controller_mock = OCMClassMock([NewTabPageViewController class]);
    coordinator_.NTPViewController =
        (NewTabPageViewController*)view_controller_mock;

    // Expect the call on the view controller.
    DynamicallyCallSelector([view_controller_mock expect],
                            view_controller_selector,
                            [NewTabPageViewController class]);

    // Call the method on the coordinator.
    DynamicallyCallSelector(coordinator_, coordinator_selector,
                            [coordinator_ class]);

    [view_controller_mock verify];
    coordinator_.NTPViewController = original_vc;
  }

  // Signs in a fake identity.
  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);
    AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
        ->SignIn(fake_identity,
                 signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  web::WebTaskEnvironment task_environment_;
  web::WebState* web_state_;
  id toolbar_delegate_;
  id delegate_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  id scene_state_;
  UIViewController* fake_feed_view_controller_;
  NewTabPageCoordinator* coordinator_;
  NewTabPageMetricsRecorder* NTPMetricsRecorder_;
  id component_factory_mock_;
  UIViewController* base_view_controller_;
  id omnibox_commands_handler_mock;
  id snackbar_commands_handler_mock;
  id fakebox_focuser_handler_mock;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the coordinator doesn't vend an IncognitoViewController VC on the
// record.
TEST_F(NewTabPageCoordinatorTest, StartOnTheRecord) {
  CreateCoordinator(/*off_the_record=*/false);
  SetupCommandHandlerMocks();
  [coordinator_ start];
  UIViewController* viewController = [coordinator_ viewController];
  EXPECT_FALSE([viewController isKindOfClass:[IncognitoViewController class]]);
  [coordinator_ stop];
}

// Tests that the coordinator vends an incognito VC off the record.
TEST_F(NewTabPageCoordinatorTest, StartOffTheRecord) {
  CreateCoordinator(/*off_the_record=*/true);
  [coordinator_ start];
  UIViewController* viewController = [coordinator_ viewController];
  EXPECT_TRUE([viewController isKindOfClass:[IncognitoViewController class]]);
  [coordinator_ stop];
}

// Tests that if the NTPCoordinator properly configures
// NewTabPageHeaderViewController and NewTabPageTabHelper correctly for
// Start depending on public lifecycle API calls.
TEST_F(NewTabPageCoordinatorTest, StartIsStartShowing) {
  CreateCoordinator(/*off_the_record=*/false);
  SetupCommandHandlerMocks();
  void (^swizzle_block)() = ^void() {
    // no-op
  };
  // Swizzle out `-configureNTPViewController` since UI code does not need to be
  // spun up for this test.
  std::unique_ptr<ScopedBlockSwizzler> configureNTPVCSwizzler =
      std::make_unique<ScopedBlockSwizzler>(
          [NewTabPageCoordinator class], @selector(configureNTPViewController),
          swizzle_block);
  // Swizzle out `-restoreNTPState` to prevent NTP VC's view from being loaded.
  std::unique_ptr<ScopedBlockSwizzler> restoreNTPStateSwizzler =
      std::make_unique<ScopedBlockSwizzler>([NewTabPageCoordinator class],
                                            @selector(restoreNTPState),
                                            swizzle_block);

  id coordinator_mock = OCMClassMock([ContentSuggestionsCoordinator class]);
  ContentSuggestionsCoordinator* mockContentSuggestionsCoordinator =
      coordinator_mock;

  // Set next NTP as start surface. Test that starting the NTP and navigating to
  // it configures the start surface.
  OCMExpect([coordinator_mock alloc]).andReturn(coordinator_mock);
  OCMExpect([coordinator_mock initWithBaseViewController:[OCMArg any]
                                                 browser:browser_.get()])
      .andReturn(mockContentSuggestionsCoordinator);
  NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(true);
  OCMExpect([coordinator_mock configureStartSurfaceIfNeeded]);
  [coordinator_ start];
  [coordinator_ didNavigateToNTPInWebState:web_state_];

  // Test opening a Start surface with another NTP tab opened in the background
  // (e.g. the NTP coordinator is already started).
  [coordinator_ didNavigateAwayFromNTP];
  [coordinator_ stop];
  OCMExpect([coordinator_mock alloc]).andReturn(coordinator_mock);
  OCMExpect([coordinator_mock initWithBaseViewController:[OCMArg any]
                                                 browser:browser_.get()])
      .andReturn(mockContentSuggestionsCoordinator);
  [coordinator_ start];
  NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(true);
  OCMExpect([coordinator_mock configureStartSurfaceIfNeeded]);
  [coordinator_ didNavigateToNTPInWebState:web_state_];
  EXPECT_OCMOCK_VERIFY(coordinator_mock);

  // Test `-didNavigateAwayFromNTPWithinWebState` when currently showing Start
  // resets the configuration.
  [coordinator_ didNavigateAwayFromNTP];
  EXPECT_FALSE(
      NewTabPageTabHelper::FromWebState(web_state_)->ShouldShowStartSurface());
  [coordinator_ stop];

  // Test the active WebState updates NTP Start state to false if it
  // began as true.
  // NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(true);
  OCMExpect([coordinator_mock alloc]).andReturn(coordinator_mock);
  OCMExpect([coordinator_mock initWithBaseViewController:[OCMArg any]
                                                 browser:browser_.get()])
      .andReturn(mockContentSuggestionsCoordinator);
  OCMExpect([coordinator_mock configureStartSurfaceIfNeeded]);
  NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(true);
  [coordinator_ start];
  [coordinator_ didNavigateToNTPInWebState:web_state_];
  EXPECT_OCMOCK_VERIFY(coordinator_mock);
  // Save reference before `web_state_` is set to new active WebState.
  web::WebState* start_web_state = web_state_;
  // Simulate the active WebState change callback.
  InsertWebState(CreateWebStateWithURL(GURL("chrome://version")));
  [coordinator_ didNavigateAwayFromNTP];
  // Moved away from Start surface to a different WebState, Start config for
  // original WebState's TabHelper should be NO.
  EXPECT_FALSE(NewTabPageTabHelper::FromWebState(start_web_state)
                   ->ShouldShowStartSurface());
  [coordinator_ stop];

  // Test `-start` doesn't set `isStartShowing` if NTPTabHelper's
  // `-ShouldShowStartSurface` is false.
  NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(false);
  [[coordinator_mock reject] configureStartSurfaceIfNeeded];
  SetNTPAsCurrentURL();
  [coordinator_ start];
  [coordinator_ didNavigateToNTPInWebState:web_state_];
  EXPECT_OCMOCK_VERIFY(coordinator_mock);
  [coordinator_ stop];
}

// Tests that tapping on the fake omnibox logs the correct metric depending on
// if Start is configured.
TEST_F(NewTabPageCoordinatorTest, FakeboxTappedMetricLogging) {
  CreateCoordinator(/*off_the_record=*/false);
  SetupCommandHandlerMocks();

  // Test `-start` sets `isStartShowing` to true/false, depending on
  // SetShowStartSurface.
  NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(true);
  [coordinator_ start];
  [coordinator_ didNavigateToNTPInWebState:web_state_];
  histogram_tester_->ExpectUniqueSample("IOS.Start.Click",
                                        IOSHomeActionType::kFakebox, 0);
  [coordinator_ fakeboxTapped];
  histogram_tester_->ExpectUniqueSample("IOS.Start.Click",
                                        IOSHomeActionType::kFakebox, 1);
  web::FakeNavigationContext navigation_context;
  navigation_context.SetUrl(GURL("chrome://version"));
  static_cast<web::FakeWebState*>(web_state_)
      ->OnNavigationStarted(&navigation_context);
  [coordinator_ didNavigateAwayFromNTP];
  [coordinator_ stopIfNeeded];
  ASSERT_FALSE(coordinator_.started);

  // Simulate navigate away and then back to non-Start NTP.
  SetNTPAsCurrentURL();
  [coordinator_ start];
  [coordinator_ didNavigateToNTPInWebState:web_state_];
  histogram_tester_->ExpectUniqueSample("IOS.Start.Click",
                                        IOSHomeActionType::kFakebox, 1);
  histogram_tester_->ExpectUniqueSample("IOS.NTP.Click",
                                        IOSHomeActionType::kFakebox, 0);
  [coordinator_ fakeboxTapped];
  histogram_tester_->ExpectUniqueSample("IOS.Start.Click",
                                        IOSHomeActionType::kFakebox, 1);
  histogram_tester_->ExpectUniqueSample("IOS.NTP.Click",
                                        IOSHomeActionType::kFakebox, 1);
  [coordinator_ didNavigateAwayFromNTP];
  [coordinator_ stop];
}

// Test that in response to tapping on MVT while on the Start Surface, the
// NTPTabHelper, NTPCoordinator, and ContentSuggestionsMediator perform as
// expected, leading to logging the correct NTP metric and resets NTPTabHelper's
// ShouldShowStartSurface() property.
TEST_F(NewTabPageCoordinatorTest, MVTStartMetricLogging) {
  CreateCoordinator(/*off_the_record=*/false);
  UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
  FakeUrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
  SetupCommandHandlerMocks();
  NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(true);
  [coordinator_ start];
  [coordinator_ didNavigateToNTPInWebState:web_state_];

  histogram_tester_->ExpectUniqueSample("IOS.Start.Click",
                                        IOSHomeActionType::kMostVisitedTile, 0);
  histogram_tester_->ExpectTotalCount(kStartTimeSpentHistogram, 0);
  histogram_tester_->ExpectTotalCount(kStartImpressionHistogram, 1);

  ContentSuggestionsMostVisitedItem* item =
      [[ContentSuggestionsMostVisitedItem alloc] init];
  item.title = @"Most Visited Index 0";
  item.URL = GURL("chrome://version");
  [coordinator_.contentSuggestionsCoordinator.contentSuggestionsMediator
      openMostVisitedItem:item
                  atIndex:0];
  // Force the URL load callback to simulate the NavigationManager receiving the
  // URL load signal from the URLLoadingBrowserAgent.
  web::FakeNavigationContext navigation_context;
  navigation_context.SetUrl(GURL("chrome://version"));
  static_cast<web::FakeWebState*>(web_state_)
      ->OnNavigationStarted(&navigation_context);
  // Simulate BrowserCoordinator receiving NTPTabHelper's
  // newTabPageHelperDidChangeVisibility: callback.
  [coordinator_ didNavigateAwayFromNTP];

  // Verify that ActionOnStartSurface metric was logged, meaning that
  // NewTabPageMetricsRecorder logged the metric before NewTabPageTabHelper
  // received the DidStartNavigation() WebStateObserver callback to reset
  // ShouldShowStartSurface() to false.
  histogram_tester_->ExpectUniqueSample("IOS.Start.Click",
                                        IOSHomeActionType::kMostVisitedTile, 1);
  histogram_tester_->ExpectTotalCount(kStartTimeSpentHistogram, 1);
  histogram_tester_->ExpectTotalCount(kStartImpressionHistogram, 1);
  EXPECT_FALSE(
      NewTabPageTabHelper::FromWebState(web_state_)->ShouldShowStartSurface());
  [coordinator_ stop];
}

TEST_F(NewTabPageCoordinatorTest, DidNavigateWithinWebState) {
  // Test normal and incognito modes.
  for (bool off_the_record : {false, true}) {
    CreateCoordinator(off_the_record);
    SetupCommandHandlerMocks();

    // Starting the NTP coordinator should not make it visible.
    [coordinator_ start];
    EXPECT_TRUE(coordinator_.started);
    EXPECT_FALSE(coordinator_.visible);

    // Navigate to NTP within the web state and check that NTP is visible.
    [coordinator_ didNavigateToNTPInWebState:web_state_];
    EXPECT_TRUE(coordinator_.started);
    EXPECT_TRUE(coordinator_.visible);

    // Simulate navigating away from the NTP.
    web::FakeNavigationContext navigation_context;
    navigation_context.SetUrl(GURL("chrome://version"));
    static_cast<web::FakeWebState*>(web_state_)
        ->OnNavigationStarted(&navigation_context);
    [coordinator_ didNavigateAwayFromNTP];

    // Remove one of the tabs so that NTPCoordinator will actually stop.
    browser_->GetWebStateList()->CloseWebStateAt(
        /*index=*/0, /* close_flags= */ 0);
    [coordinator_ stopIfNeeded];
    EXPECT_FALSE(coordinator_.started);
    EXPECT_FALSE(coordinator_.visible);
  }
}

TEST_F(NewTabPageCoordinatorTest, DidNavigateBetweenWebStates) {
  // Test normal and incognito modes.
  for (bool off_the_record : {false, true}) {
    CreateCoordinator(off_the_record);
    SetupCommandHandlerMocks();

    // Starting the NTP coordinator should not make it visible.
    [coordinator_ start];
    EXPECT_TRUE(coordinator_.started);
    EXPECT_FALSE(coordinator_.visible);
    if (!off_the_record) {
      histogram_tester_->ExpectTotalCount(kNTPImpressionHistogram, 0);
    }

    // Open an NTP in a new web state.
    InsertWebState(CreateWebStateWithURL(GURL("chrome://newtab")));
    [coordinator_ didNavigateToNTPInWebState:web_state_];
    if (!off_the_record) {
      histogram_tester_->ExpectTotalCount(kNTPTimeSpentHistogram, 0);
      histogram_tester_->ExpectTotalCount(kNTPImpressionHistogram, 1);
    }
    EXPECT_TRUE(coordinator_.started);
    EXPECT_TRUE(coordinator_.visible);

    // Insert a non-NTP WebState.
    InsertWebState(CreateWebStateWithURL(GURL("chrome://version")));
    [coordinator_ didNavigateAwayFromNTP];
    if (!off_the_record) {
      histogram_tester_->ExpectTotalCount(kNTPTimeSpentHistogram, 1);
      histogram_tester_->ExpectTotalCount(kNTPImpressionHistogram, 1);
    }
    EXPECT_TRUE(coordinator_.started);
    EXPECT_FALSE(coordinator_.visible);

    // Close non-NTP web state to get back to NTP web state.
    browser_->GetWebStateList()->CloseWebStateAt(
        /*index=*/1, /* close_flags= */ 0);
    [coordinator_ didNavigateToNTPInWebState:web_state_];
    if (!off_the_record) {
      histogram_tester_->ExpectTotalCount(kNTPTimeSpentHistogram, 1);
      histogram_tester_->ExpectTotalCount(kNTPImpressionHistogram, 2);
    }
    EXPECT_TRUE(coordinator_.started);
    EXPECT_TRUE(coordinator_.visible);

    // Close all web states.
    [coordinator_ didNavigateAwayFromNTP];
    browser_->GetWebStateList()->CloseAllWebStates(
        WebStateList::CLOSE_NO_FLAGS);
    [coordinator_ stopIfNeeded];
    if (!off_the_record) {
      histogram_tester_->ExpectTotalCount(kNTPTimeSpentHistogram, 2);
      histogram_tester_->ExpectTotalCount(kNTPImpressionHistogram, 2);
    }
    EXPECT_FALSE(coordinator_.visible);
    EXPECT_FALSE(coordinator_.started);
  }
}

// Tests that various NTPCoordinator methods correctly proxy method calls to
// the NTPViewController.
TEST_F(NewTabPageCoordinatorTest, ProxiesNTPViewControllerMethods) {
  CreateCoordinator(/*off_the_record=*/false);
  SetupCommandHandlerMocks();
  [coordinator_ start];
  [coordinator_ didNavigateToNTPInWebState:web_state_];

  ExpectMethodToProxyToVC(@selector(stopScrolling), @selector(stopScrolling));
  ExpectMethodToProxyToVC(@selector(isScrolledToTop),
                          @selector(isNTPScrolledToTop));
  ExpectMethodToProxyToVC(@selector(willUpdateSnapshot),
                          @selector(willUpdateSnapshot));
  ExpectMethodToProxyToVC(@selector(focusFakebox), @selector(focusOmnibox));
  ExpectMethodToProxyToVC(@selector(locationBarDidResignFirstResponder),
                          @selector(omniboxDidResignFirstResponder));

  [coordinator_ stop];
}

// Tests the state of the NTP coordinator after starting and stopping it. This
// mainly ensures that all strongly references properties are created and
// released, but also checks that the NTP state is correct for each scnenario.
TEST_F(NewTabPageCoordinatorTest, IsNTPCleanOnStop) {
  CreateCoordinator(/*off_the_record=*/false);
  SetupCommandHandlerMocks();

  [coordinator_ start];
  EXPECT_NE(nil, coordinator_.NTPViewController);
  EXPECT_NE(nil, coordinator_.contentSuggestionsCoordinator.viewController);
  EXPECT_NE(nil, coordinator_.contentSuggestionsCoordinator);
  EXPECT_NE(nil, coordinator_.headerViewController);
  EXPECT_NE(nil, coordinator_.NTPMediator);
  EXPECT_NE(nil, coordinator_.feedWrapperViewController);
  EXPECT_NE(nil, coordinator_.feedTopSectionCoordinator);
  EXPECT_NE(nil, coordinator_.feedHeaderViewController);
  EXPECT_TRUE(coordinator_.started);

  [coordinator_ stop];
  EXPECT_EQ(nil, coordinator_.NTPViewController);
  EXPECT_EQ(nil, coordinator_.contentSuggestionsCoordinator.viewController);
  EXPECT_EQ(nil, coordinator_.contentSuggestionsCoordinator);
  EXPECT_EQ(nil, coordinator_.headerViewController);
  EXPECT_EQ(nil, coordinator_.NTPMediator);
  EXPECT_EQ(nil, coordinator_.feedWrapperViewController);
  EXPECT_EQ(nil, coordinator_.feedTopSectionCoordinator);
  EXPECT_EQ(nil, coordinator_.feedHeaderViewController);
  EXPECT_FALSE(coordinator_.started);
}

// Tests that the state of an NTP is saved and restored when leaving an NTP and
// navigating back to it in the same web state.
TEST_F(NewTabPageCoordinatorTest, TestSaveNTPState) {
  CreateCoordinator(/*off_the_record=*/false);
  SetupCommandHandlerMocks();
  [coordinator_ start];
  [coordinator_ didNavigateToNTPInWebState:web_state_];

  // Check that initial NTP is scrolled to top.
  EXPECT_NEAR(coordinator_.NTPViewController.scrollPosition,
              -[coordinator_.NTPViewController heightAboveFeed], 1);

  // Change the selected feed and set some scroll position.
  [coordinator_ selectFeedType:FeedTypeFollowing];
  [coordinator_.NTPViewController setContentOffsetToTopOfFeedOrLess:500];

  FeedType selectedFeed = coordinator_.selectedFeed;
  CGFloat scrollPosition = coordinator_.NTPViewController.scrollPosition;

  // Navigate away from the NTP and stop the coordinator.
  [coordinator_ didNavigateAwayFromNTP];
  [coordinator_ stop];

  // Navigate to another NTP in the same web state.
  [coordinator_ start];
  [coordinator_ didNavigateToNTPInWebState:web_state_];

  // Check that newly opened NTP restores saved state.
  EXPECT_EQ(coordinator_.selectedFeed, selectedFeed);
  EXPECT_NEAR(coordinator_.NTPViewController.scrollPosition, scrollPosition, 1);

  [coordinator_ stop];
}

// Tests that following feed and discover feed can be selected.
TEST_F(NewTabPageCoordinatorTest, SelectFeedType) {
  // Following feed is only available in the US, so we need to override the
  // VariationsService's stored permenant country to test.
  ScopedVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");

  CreateCoordinator(/*off_the_record=*/false);
  [coordinator_ start];
  // Simulate the view appearing.
  [coordinator_.NTPViewController beginAppearanceTransition:YES animated:NO];
  [coordinator_.NTPViewController endAppearanceTransition];
  SignIn();
  // Scroll down slightly.
  CGFloat scrollPosition =
      round(coordinator_.NTPViewController.scrollPosition + 100);
  [coordinator_.NTPViewController
      setContentOffsetToTopOfFeedOrLess:scrollPosition];

  // Expect the Following feed to be loaded, and scroll position to be
  // maintained.
  OCMExpect([component_factory_mock_
                    followingFeedForBrowser:browser_.get()
                viewControllerConfiguration:[OCMArg any]
                                   sortType:FollowingFeedSortTypeByLatest])
      .andReturn(fake_feed_view_controller_);
  [coordinator_ selectFeedType:FeedTypeFollowing];
  EXPECT_OCMOCK_VERIFY(component_factory_mock_);
  EXPECT_EQ(coordinator_.selectedFeed, FeedTypeFollowing);
  EXPECT_EQ(coordinator_.NTPViewController.scrollPosition, scrollPosition);

  // Expect the Discover feed to be loaded, and scroll position to be
  // maintained.
  OCMExpect([component_factory_mock_ discoverFeedForBrowser:browser_.get()
                                viewControllerConfiguration:[OCMArg any]])
      .andReturn(fake_feed_view_controller_);
  [coordinator_ selectFeedType:FeedTypeDiscover];
  EXPECT_OCMOCK_VERIFY(component_factory_mock_);
  EXPECT_EQ(coordinator_.selectedFeed, FeedTypeDiscover);
  EXPECT_EQ(coordinator_.NTPViewController.scrollPosition, scrollPosition);

  [coordinator_ stop];
}
