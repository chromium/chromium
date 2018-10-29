// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#include "base/test/scoped_task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"

#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@protocol NewTabPageTabDispatcher<ApplicationCommands,
                                  BrowserCommands,
                                  OmniboxFocuser,
                                  FakeboxFocuser,
                                  SnackbarCommands,
                                  UrlLoader>
@end

// Test fixture for testing NewTabPageCoordinator class.
class NewTabPageCoordinatorTest : public PlatformTest {
 protected:
  NewTabPageCoordinatorTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())) {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeContentSuggestionsServiceFactory::GetInstance(),
        IOSChromeContentSuggestionsServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    browser_state_ = test_cbs_builder.Build();

    loader_ = OCMProtocolMock(@protocol(UrlLoader));
    toolbar_delegate_ =
        OCMProtocolMock(@protocol(NewTabPageControllerDelegate));
    dispatcher_ = OCMProtocolMock(@protocol(NewTabPageTabDispatcher));
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
  }

  void CreateCoordinator(bool off_the_record) {
    if (off_the_record) {
      ios::ChromeBrowserState* otr_state =
          browser_state_->GetOffTheRecordChromeBrowserState();
      coordinator_ =
          [[NewTabPageCoordinator alloc] initWithBrowserState:otr_state];
    } else {
      coordinator_ = [[NewTabPageCoordinator alloc]
          initWithBrowserState:browser_state_.get()];
    }
    coordinator_.URLLoader = loader_;
    coordinator_.toolbarDelegate = toolbar_delegate_;
    coordinator_.dispatcher = dispatcher_;
    coordinator_.webStateList = web_state_list_.get();
  }

  id dispatcher_;
  id toolbar_delegate_;
  id loader_;
  id delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  NewTabPageCoordinator* coordinator_;
};

// Tests that the coordinator vends a content suggestions VC on the record.
TEST_F(NewTabPageCoordinatorTest, StartOnTheRecord) {
  CreateCoordinator(/*off_the_record=*/false);
  [coordinator_ start];
  UIViewController* viewController = [coordinator_ viewController];
  EXPECT_TRUE(
      [viewController isKindOfClass:[ContentSuggestionsViewController class]]);
}

// Tests that the coordinator vends an incognito VC off the record.
TEST_F(NewTabPageCoordinatorTest, StartOffTheRecord) {
  CreateCoordinator(/*off_the_record=*/true);
  [coordinator_ start];
  UIViewController* viewController = [coordinator_ viewController];
  EXPECT_TRUE([viewController isKindOfClass:[IncognitoViewController class]]);
}
