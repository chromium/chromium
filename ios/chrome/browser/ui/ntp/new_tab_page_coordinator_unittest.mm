// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/ntp/incognito/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator+private.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

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
    NewTabPageTabHelper::CreateForWebState(&web_state_);
  }

  void CreateCoordinator(bool off_the_record) {
    if (off_the_record) {
      ChromeBrowserState* otr_state =
          browser_state_->GetOffTheRecordChromeBrowserState();
      browser_ = std::make_unique<TestBrowser>(otr_state);
    } else {
      browser_ = std::make_unique<TestBrowser>(browser_state_.get());
      scene_state_ = OCMClassMock([SceneState class]);
      SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
      StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser_.get());
    }
    NewTabPageTabHelper::CreateForWebState(&web_state_);
    coordinator_ =
        [[NewTabPageCoordinator alloc] initWithBrowser:browser_.get()];
    coordinator_.baseViewController = base_view_controller_;
    coordinator_.toolbarDelegate = toolbar_delegate_;
    coordinator_.webState = &web_state_;
  }

  web::FakeWebState web_state_;
  id toolbar_delegate_;
  id delegate_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  id scene_state_;
  NewTabPageCoordinator* coordinator_;
  UIViewController* base_view_controller_;
};

// Tests that the coordinator doesn't vend an IncognitoViewController VC on the
// record.
TEST_F(NewTabPageCoordinatorTest, StartOnTheRecord) {
  CreateCoordinator(/*off_the_record=*/false);
  id omniboxCommandsHandlerMock = OCMProtocolMock(@protocol(OmniboxCommands));
  id snackbarCommandsHandlerMock = OCMProtocolMock(@protocol(SnackbarCommands));
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:omniboxCommandsHandlerMock
                   forProtocol:@protocol(OmniboxCommands)];
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:snackbarCommandsHandlerMock
                   forProtocol:@protocol(SnackbarCommands)];
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
}

// Tests that if the NTP should/shouldn't be showing Start upon -start, that it
// properly configures the ContentSuggestionsHeaderViewController property.
TEST_F(NewTabPageCoordinatorTest, StartIsStartShowing) {
  NewTabPageTabHelper::FromWebState(&web_state_)->SetShowStartSurface(true);
  CreateCoordinator(/*off_the_record=*/false);
  id omniboxCommandsHandlerMock = OCMProtocolMock(@protocol(OmniboxCommands));
  id snackbarCommandsHandlerMock = OCMProtocolMock(@protocol(SnackbarCommands));
  id fakeboxFocuserHandlerMock = OCMProtocolMock(@protocol(FakeboxFocuser));

  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:omniboxCommandsHandlerMock
                   forProtocol:@protocol(OmniboxCommands)];
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:snackbarCommandsHandlerMock
                   forProtocol:@protocol(SnackbarCommands)];
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:fakeboxFocuserHandlerMock
                   forProtocol:@protocol(FakeboxFocuser)];

  [coordinator_ start];
  EXPECT_TRUE(coordinator_.headerController.isStartShowing);
  [coordinator_ stop];

  NewTabPageTabHelper::FromWebState(&web_state_)->SetShowStartSurface(false);
  [coordinator_ start];
  EXPECT_FALSE(coordinator_.headerController.isStartShowing);
  [coordinator_ stop];
}

// Tests that calls to the coordinator's -ntpDidChangeVisibility: passing YES
// when Start should also show updates the state of the
// ContentSuggestionsHeaderViewController and passing NO when Start was showing
// updates ContentSuggestionsHeaderViewController and NewTabPageTabHelper
// correctly.
TEST_F(NewTabPageCoordinatorTest, DidChangeVisibility) {
  CreateCoordinator(/*off_the_record=*/false);
  id omniboxCommandsHandlerMock = OCMProtocolMock(@protocol(OmniboxCommands));
  id snackbarCommandsHandlerMock = OCMProtocolMock(@protocol(SnackbarCommands));
  id fakeboxFocuserHandlerMock = OCMProtocolMock(@protocol(FakeboxFocuser));
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:omniboxCommandsHandlerMock
                   forProtocol:@protocol(OmniboxCommands)];
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:snackbarCommandsHandlerMock
                   forProtocol:@protocol(SnackbarCommands)];
  [browser_.get()->GetCommandDispatcher()
      startDispatchingToTarget:fakeboxFocuserHandlerMock
                   forProtocol:@protocol(FakeboxFocuser)];
  [coordinator_ start];
  EXPECT_FALSE(coordinator_.headerController.isStartShowing);

  NewTabPageTabHelper::FromWebState(&web_state_)->SetShowStartSurface(true);
  [coordinator_ ntpDidChangeVisibility:YES];
  EXPECT_TRUE(coordinator_.headerController.isStartShowing);

  [coordinator_ ntpDidChangeVisibility:NO];
  EXPECT_FALSE(
      NewTabPageTabHelper::FromWebState(&web_state_)->ShouldShowStartSurface());
  EXPECT_FALSE(coordinator_.headerController.isStartShowing);

  [coordinator_ stop];
}
