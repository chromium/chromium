// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_mediator.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// A random scroll position value to use for testing.
const CGFloat kSomeScrollPosition = 500.0;
}  // namespace

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
                initWithWebState:initial_web_state_.get()
              templateURLService:ios::TemplateURLServiceFactory::
                                     GetForBrowserState(
                                         chrome_browser_state_.get())
                       URLLoader:url_loader_
                     authService:auth_service_
                 identityManager:identity_manager_
           accountManagerService:account_manager_service
                      logoVendor:logo_vendor_
        identityDiscImageUpdater:image_updater_
                     isIncognito:is_incognito
             discoverFeedService:discover_feed_service];
    header_consumer_ = OCMProtocolMock(@protocol(NewTabPageHeaderConsumer));
    mediator_.headerConsumer = header_consumer_;
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
    NewTabPageTabHelper::FromWebState(web_state.get())
        ->SaveNTPState(scroll_position, NewTabPageTabHelper::DefaultFeedType());
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
  OCMExpect([header_consumer_ setLogoVendor:logo_vendor_]);
  OCMExpect([header_consumer_ setLogoIsShowing:YES]);

  // Action.
  [mediator_ setUp];

  // Tests.
  EXPECT_OCMOCK_VERIFY(header_consumer_);
}

// Tests that the the mediator calls the consumer to set the content offset,
// when a new WebState is set.
TEST_F(NewTabPageMediatorTest, TestSetContentOffsetForWebState) {
  id suggestions_mediator = OCMClassMock([ContentSuggestionsMediator class]);
  mediator_.suggestionsMediator = suggestions_mediator;
  [mediator_ setUp];

  // Test with WebState that has not been scrolled.
  id ntp_consumer = SetupNTPConsumerMock();
  std::unique_ptr<web::WebState> web_state_1 =
      CreateWebStateWithURL(GURL("chrome://newtab"), 0.0);
  OCMExpect([ntp_consumer setContentOffsetToTop]);
  [[[ntp_consumer reject] ignoringNonObjectArgs] setSavedContentOffset:0];
  OCMExpect([suggestions_mediator refreshMostVisitedTiles]);
  mediator_.webState = web_state_1.get();
  EXPECT_OCMOCK_VERIFY(ntp_consumer);
  EXPECT_OCMOCK_VERIFY(suggestions_mediator);

  // Test with WebState that is scrolled down some.
  ntp_consumer = SetupNTPConsumerMock();
  std::unique_ptr<web::WebState> web_state_2 =
      CreateWebStateWithURL(GURL("chrome://newtab"), kSomeScrollPosition);
  OCMExpect([ntp_consumer setSavedContentOffset:kSomeScrollPosition]);
  [[ntp_consumer reject] setContentOffsetToTop];
  mediator_.webState = web_state_2.get();
  EXPECT_OCMOCK_VERIFY(ntp_consumer);
}

// Tests that the mediator saves the current content offset for a WebState when
// a new one is assigned.
TEST_F(NewTabPageMediatorTest, TestSaveContentOffsetForWebState) {
  id ntp_consumer = OCMProtocolMock(@protocol(NewTabPageConsumer));
  [[[ntp_consumer expect] andReturnValue:[NSNumber numberWithDouble:0.0]]
      heightAboveFeed];
  mediator_.consumer = ntp_consumer;
  [mediator_ setUp];

  std::unique_ptr<web::WebState> web_state_1 =
      CreateWebStateWithURL(GURL("chrome://newtab"));
  [[[ntp_consumer expect]
      andReturnValue:[NSNumber numberWithDouble:kSomeScrollPosition]]
      scrollPosition];
  [[[ntp_consumer expect] andReturnValue:[NSNumber numberWithDouble:0.0]]
      collectionShiftingOffset];
  mediator_.webState = web_state_1.get();
  EXPECT_OCMOCK_VERIFY(ntp_consumer);
  CGFloat saved_scroll_position =
      NewTabPageTabHelper::FromWebState(initial_web_state_.get())
          ->ScrollPositionFromSavedState();
  EXPECT_EQ(saved_scroll_position, kSomeScrollPosition);
}
