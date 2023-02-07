// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_mediator.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/ntp/logo_vendor.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
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
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    NewTabPageTabHelper::CreateForWebState(fake_web_state_.get());
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
    ChromeAccountManagerService* accountManagerService =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    imageUpdater_ = OCMProtocolMock(@protocol(UserAccountImageUpdateDelegate));
    mediator_ = [[NewTabPageMediator alloc]
                initWithWebState:fake_web_state_.get()
              templateURLService:ios::TemplateURLServiceFactory::
                                     GetForBrowserState(
                                         chrome_browser_state_.get())
                       URLLoader:url_loader_
                     authService:auth_service_
                 identityManager:identity_manager_
           accountManagerService:accountManagerService
                      logoVendor:logo_vendor_
        identityDiscImageUpdater:imageUpdater_];
    contentSuggestionsHeaderConsumer_ =
        OCMProtocolMock(@protocol(ContentSuggestionsHeaderConsumer));
    mediator_.contentSuggestionsHeaderConsumer =
        contentSuggestionsHeaderConsumer_;
    histogram_tester_.reset(new base::HistogramTester());
  }

  // Explicitly disconnect the mediator.
  ~NewTabPageMediatorTest() override { [mediator_ shutdown]; }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  id contentSuggestionsHeaderConsumer_;
  id imageUpdater_;
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
  OCMExpect([contentSuggestionsHeaderConsumer_ setLogoVendor:logo_vendor_]);
  OCMExpect([contentSuggestionsHeaderConsumer_ setLogoIsShowing:YES]);

  // Action.
  [mediator_ setUp];

  // Tests.
  EXPECT_OCMOCK_VERIFY(contentSuggestionsHeaderConsumer_);
}
