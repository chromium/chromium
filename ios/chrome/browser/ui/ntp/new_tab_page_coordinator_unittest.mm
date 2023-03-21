// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/ntp/incognito/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_component_factory.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator+private.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  }

  void CreateCoordinator(bool off_the_record) {
    if (off_the_record) {
      ChromeBrowserState* otr_state =
          browser_state_->GetOffTheRecordChromeBrowserState();
      browser_ = std::make_unique<TestBrowser>(otr_state);
    } else {
      browser_ = std::make_unique<TestBrowser>(browser_state_.get());
      StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());
    }
    scene_state_ = OCMClassMock([SceneState class]);
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

    coordinator_ = [[NewTabPageCoordinator alloc]
         initWithBrowser:browser_.get()
        componentFactory:[[NewTabPageComponentFactory alloc] init]];
    coordinator_.baseViewController = base_view_controller_;
    coordinator_.toolbarDelegate = toolbar_delegate_;

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

  web::WebTaskEnvironment task_environment_;
  web::WebState* web_state_;
  id toolbar_delegate_;
  id delegate_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  id scene_state_;
  NewTabPageCoordinator* coordinator_;
  UIViewController* base_view_controller_;
  id omnibox_commands_handler_mock;
  id snackbar_commands_handler_mock;
  id fakebox_focuser_handler_mock;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
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
// ContentSuggestionsHeaderViewController and NewTabPageTabHelper correctly for
// Start depending on public lifecycle API calls.
TEST_F(NewTabPageCoordinatorTest, StartIsStartShowing) {
  CreateCoordinator(/*off_the_record=*/false);
  SetupCommandHandlerMocks();
  // Swizzle out `-configureNTPViewController` since it leaves a dangling
  // pointer somewhere, and UI code does not need to be spun up for this test.
  void (^swizzle_block)() = ^void() {
    // no-op
  };
  std::unique_ptr<ScopedBlockSwizzler> service_swizzler =
      std::make_unique<ScopedBlockSwizzler>(
          [NewTabPageCoordinator class], @selector(configureNTPViewController),
          swizzle_block);

  id coordinator_mock = OCMClassMock([ContentSuggestionsCoordinator class]);
  ContentSuggestionsCoordinator* mockContentSuggestionsCoordinator =
      coordinator_mock;

  // Test `-start` sets `isStartShowing` to true if NTPTabHelper's
  // `-ShouldShowStartSurface` is true.
  OCMExpect([coordinator_mock alloc]).andReturn(coordinator_mock);
  OCMExpect([coordinator_mock initWithBaseViewController:[OCMArg any]
                                                 browser:browser_.get()])
      .andReturn(mockContentSuggestionsCoordinator);
  NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(true);
  OCMExpect([coordinator_mock configureStartSurfaceIfNeeded]);
  [coordinator_ start];
  EXPECT_OCMOCK_VERIFY(coordinator_mock);
  [coordinator_ stop];

  // Test `-didNavigateToNTP` configures the NTP for Start. `-didNavigateToNTP`
  // should only be called if the NTPCoordinator is still started (e.g. another
  // tab has the NTP open).
  OCMExpect([coordinator_mock alloc]).andReturn(coordinator_mock);
  OCMExpect([coordinator_mock initWithBaseViewController:[OCMArg any]
                                                 browser:browser_.get()])
      .andReturn(mockContentSuggestionsCoordinator);
  [coordinator_ start];
  NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(true);
  OCMExpect([coordinator_mock configureStartSurfaceIfNeeded]);
  [coordinator_ didNavigateToNTP];
  EXPECT_OCMOCK_VERIFY(coordinator_mock);
  // Test `-didNavigateAwayFromNTP` when currently showing Start resets the
  // configuration.
  [coordinator_ didNavigateAwayFromNTP];
  EXPECT_FALSE(
      NewTabPageTabHelper::FromWebState(web_state_)->ShouldShowStartSurface());
  [coordinator_ stop];

  // Test `-didChangeActiveWebState:` updates NTP Start state to false if it
  // began as true.
  NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(true);
  OCMExpect([coordinator_mock alloc]).andReturn(coordinator_mock);
  OCMExpect([coordinator_mock initWithBaseViewController:[OCMArg any]
                                                 browser:browser_.get()])
      .andReturn(mockContentSuggestionsCoordinator);
  OCMExpect([coordinator_mock configureStartSurfaceIfNeeded]);
  [coordinator_ start];
  EXPECT_OCMOCK_VERIFY(coordinator_mock);
  // Save reference before `web_state_` is set to new active WebState.
  web::WebState* start_web_state = web_state_;
  // Simulate didChangeActiveWebState: callback.
  InsertWebState(CreateWebStateWithURL(GURL("chrome://version")));
  // Moved away from Start surface to a different WebState, Start config for
  // original WebState's TabHelper should be NO.
  EXPECT_FALSE(NewTabPageTabHelper::FromWebState(start_web_state)
                   ->ShouldShowStartSurface());
  [coordinator_ stop];

  // Test `-start` doesn't set `isStartShowing` if NTPTabHelper's
  // `-ShouldShowStartSurface` is false.
  NewTabPageTabHelper::FromWebState(web_state_)->SetShowStartSurface(false);
  [[coordinator_mock reject] configureStartSurfaceIfNeeded];
  [coordinator_ start];
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
  histogram_tester_->ExpectUniqueSample(
      "IOS.ContentSuggestions.ActionOnStartSurface",
      IOSContentSuggestionsActionType::kFakebox, 0);
  [coordinator_ fakeboxTapped];
  histogram_tester_->ExpectUniqueSample(
      "IOS.ContentSuggestions.ActionOnStartSurface",
      IOSContentSuggestionsActionType::kFakebox, 1);

  // Simulate navigate away and then back to non-Start NTP.
  web::FakeNavigationContext navigation_context;
  navigation_context.SetUrl(GURL("chrome://version"));
  static_cast<web::FakeWebState*>(web_state_)
      ->OnNavigationStarted(&navigation_context);
  [coordinator_ didNavigateAwayFromNTP];
  ASSERT_FALSE(coordinator_.started);
  [coordinator_ start];
  histogram_tester_->ExpectUniqueSample(
      "IOS.ContentSuggestions.ActionOnStartSurface",
      IOSContentSuggestionsActionType::kFakebox, 1);
  histogram_tester_->ExpectUniqueSample(
      "IOS.ContentSuggestions.ActionOnNTP",
      IOSContentSuggestionsActionType::kFakebox, 0);
  [coordinator_ fakeboxTapped];
  histogram_tester_->ExpectUniqueSample(
      "IOS.ContentSuggestions.ActionOnStartSurface",
      IOSContentSuggestionsActionType::kFakebox, 1);
  histogram_tester_->ExpectUniqueSample(
      "IOS.ContentSuggestions.ActionOnNTP",
      IOSContentSuggestionsActionType::kFakebox, 1);
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

  histogram_tester_->ExpectUniqueSample(
      "IOS.ContentSuggestions.ActionOnStartSurface",
      IOSContentSuggestionsActionType::kMostVisitedTile, 0);

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
  // NTPHomeMetrics logged the metric before NewTabPageTabHelper received the
  // DidStartNavigation() WebStateObserver callback to reset
  // ShouldShowStartSurface() to false.
  histogram_tester_->ExpectUniqueSample(
      "IOS.ContentSuggestions.ActionOnStartSurface",
      IOSContentSuggestionsActionType::kMostVisitedTile, 1);
  EXPECT_FALSE(
      NewTabPageTabHelper::FromWebState(web_state_)->ShouldShowStartSurface());
  [coordinator_ stop];
}

// Test -didNavigateToNTP to simulate the user navigating back to the NTP, and
// -didNavigateAwayFromNTP to simulate the user navigating away from the NTP.
TEST_F(NewTabPageCoordinatorTest, DidNavigate) {
  CreateCoordinator(/*off_the_record=*/false);
  SetupCommandHandlerMocks();
  [coordinator_ start];
  [coordinator_ sceneState:nil
      transitionedToActivationLevel:SceneActivationLevelForegroundInactive];
  EXPECT_TRUE(coordinator_.visible);

  // Create second NTP since `-didNavigateToNTP` should only be called instead
  // of `-start` when there is another tab showing the NTP.
  InsertWebState(CreateWebStateWithURL(GURL("chrome://newtab")));
  // Simulate navigating away from the NTP.
  web::FakeNavigationContext navigation_context;
  navigation_context.SetUrl(GURL("chrome://version"));
  static_cast<web::FakeWebState*>(web_state_)
      ->OnNavigationStarted(&navigation_context);
  [coordinator_ didNavigateAwayFromNTP];
  ASSERT_TRUE(coordinator_.started);
  EXPECT_EQ(coordinator_.webState, nullptr);
  EXPECT_FALSE(coordinator_.visible);

  // Simulate navigating back to the NTP.
  [coordinator_ didNavigateToNTP];
  EXPECT_EQ(coordinator_.webState, web_state_);
  EXPECT_TRUE(coordinator_.visible);

  // Remove one of the tabs so that NTPCoordinator will actually stop.
  browser_->GetWebStateList()->CloseWebStateAt(
      /*index=*/0, /* close_flags= */ 0);
  web_state_ = browser_->GetWebStateList()->GetActiveWebState();
  [coordinator_ stopIfNeeded];
  ASSERT_FALSE(coordinator_.started);
}

// Test that NTPCoordinator's DidChangeActiveWebState will change the
// `webState` property as well as the NTP's visibility appropriately.
TEST_F(NewTabPageCoordinatorTest, DidChangeActiveWebState) {
  // Test normal and incognito modes.
  for (bool off_the_record : {false, true}) {
    CreateCoordinator(off_the_record);
    SetupCommandHandlerMocks();
    [coordinator_ start];
    [coordinator_ sceneState:nil
        transitionedToActivationLevel:SceneActivationLevelForegroundInactive];
    EXPECT_TRUE(coordinator_.visible);

    // Insert a non-NTP WebState.
    InsertWebState(CreateWebStateWithURL(GURL("chrome://version")));
    EXPECT_EQ(coordinator_.webState, nullptr);
    EXPECT_FALSE(coordinator_.visible);

    // Insert an NTP webstate.
    InsertWebState(CreateWebStateWithURL(GURL("chrome://newtab")));
    EXPECT_EQ(coordinator_.webState, web_state_);
    EXPECT_TRUE(coordinator_.visible);

    [coordinator_ stop];
    EXPECT_EQ(coordinator_.webState, nullptr);
    coordinator_ = nil;
  }
}

// Tests that various NTPCoordinator methods correctly proxy method calls to
// the NTPViewController.
TEST_F(NewTabPageCoordinatorTest, ProxiesNTPViewControllerMethods) {
  CreateCoordinator(/*off_the_record=*/false);
  SetupCommandHandlerMocks();
  [coordinator_ start];

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
