// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_mediator.h"

#include <memory>

#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_consumer.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_notification_names.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#include "ios/chrome/browser/url_loading/test_url_loading_service.h"
#include "ios/chrome/browser/url_loading/url_loading_params.h"
#include "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#import "ios/public/provider/chrome/browser/ui/logo_vendor.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@protocol NTPHomeMediatorDispatcher <BrowserCommands, SnackbarCommands>
@end

class NTPHomeMediatorTest : public PlatformTest {
 public:
  NTPHomeMediatorTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeContentSuggestionsServiceFactory::GetInstance(),
        IOSChromeContentSuggestionsServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        UrlLoadingServiceFactory::GetInstance(),
        UrlLoadingServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    chrome_browser_state_ = test_cbs_builder.Build();

    std::unique_ptr<ToolbarTestNavigationManager> navigation_manager =
        std::make_unique<ToolbarTestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    test_web_state_ = std::make_unique<web::TestWebState>();
    logo_vendor_ = OCMProtocolMock(@protocol(LogoVendor));
    dispatcher_ = OCMProtocolMock(@protocol(NTPHomeMediatorDispatcher));
    suggestions_view_controller_ =
        OCMClassMock([ContentSuggestionsViewController class]);
    url_loader_ =
        (TestUrlLoadingService*)UrlLoadingServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    auth_service_ = static_cast<AuthenticationServiceFake*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get()));
    identity_manager_ =
        IdentityManagerFactory::GetForBrowserState(chrome_browser_state_.get());
    mediator_ = [[NTPHomeMediator alloc]
          initWithWebState:test_web_state_.get()
        templateURLService:ios::TemplateURLServiceFactory::GetForBrowserState(
                               chrome_browser_state_.get())
         urlLoadingService:url_loader_
               authService:auth_service_
           identityManager:identity_manager_
                logoVendor:logo_vendor_];
    mediator_.suggestionsService =
        IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    mediator_.dispatcher = dispatcher_;
    mediator_.suggestionsViewController = suggestions_view_controller_;
    consumer_ = OCMProtocolMock(@protocol(NTPHomeConsumer));
    mediator_.consumer = consumer_;
  }

  // Explicitly disconnect the mediator.
  ~NTPHomeMediatorTest() override { [mediator_ shutdown]; }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  id consumer_;
  id logo_vendor_;
  id dispatcher_;
  id suggestions_view_controller_;
  NTPHomeMediator* mediator_;
  ToolbarTestNavigationManager* navigation_manager_;
  TestUrlLoadingService* url_loader_;
  AuthenticationServiceFake* auth_service_;
  signin::IdentityManager* identity_manager_;

 private:
  std::unique_ptr<web::TestWebState> test_web_state_;
};

// Tests that the consumer has the right value set up.
TEST_F(NTPHomeMediatorTest, TestConsumerSetup) {
  // Setup.
  OCMExpect([consumer_ setLogoVendor:logo_vendor_]);
  OCMExpect([consumer_ setLogoIsShowing:YES]);

  // Action.
  [mediator_ setUp];

  // Tests.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified when the location bar is focused.
TEST_F(NTPHomeMediatorTest, TestConsumerNotificationFocus) {
  // Setup.
  [mediator_ setUp];

  OCMExpect([consumer_ locationBarBecomesFirstResponder]);

  // Action.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kLocationBarBecomesFirstResponderNotification
                    object:nil];

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified when the location bar is unfocused.
TEST_F(NTPHomeMediatorTest, TestConsumerNotificationUnfocus) {
  // Setup.
  [mediator_ setUp];

  OCMExpect([consumer_ locationBarResignsFirstResponder]);

  // Action.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kLocationBarResignsFirstResponderNotification
                    object:nil];

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the command is sent to the dispatcher when opening the Reading
// List.
TEST_F(NTPHomeMediatorTest, TestOpenReadingList) {
  // Setup.
  [mediator_ setUp];
  OCMExpect([dispatcher_ showReadingList]);

  // Action.
  [mediator_ openReadingList];

  // Test.
  EXPECT_OCMOCK_VERIFY(dispatcher_);
}

// Tests that the command is sent to the loader when opening a suggestion.
TEST_F(NTPHomeMediatorTest, TestOpenPage) {
  // Setup.
  [mediator_ setUp];
  GURL url = GURL("http://chromium.org");
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0 inSection:0];
  ContentSuggestionsItem* item =
      [[ContentSuggestionsItem alloc] initWithType:0
                                             title:@"test item"
                                               url:url];
  id model = OCMClassMock([CollectionViewModel class]);
  OCMStub([suggestions_view_controller_ collectionViewModel]).andReturn(model);
  OCMStub([model itemAtIndexPath:indexPath]).andReturn(item);

  // Action.
  [mediator_ openPageForItemAtIndexPath:indexPath];

  // Test.
  EXPECT_EQ(url, url_loader_->last_params.web_params.url);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      url_loader_->last_params.web_params.transition_type));
}

// Tests that the command is sent to the loader when opening a most visited.
TEST_F(NTPHomeMediatorTest, TestOpenMostVisited) {
  // Setup.
  [mediator_ setUp];
  GURL url = GURL("http://chromium.org");
  ContentSuggestionsMostVisitedItem* item =
      [[ContentSuggestionsMostVisitedItem alloc] initWithType:0];
  item.URL = url;

  // Action.
  [mediator_ openMostVisitedItem:item atIndex:0];

  // Test.
  EXPECT_EQ(url, url_loader_->last_params.web_params.url);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      url_loader_->last_params.web_params.transition_type));
}
