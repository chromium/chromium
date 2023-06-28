// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>

#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_view.h"
#import "ios/chrome/browser/ui/tabs/tab_view.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class TabStripControllerTest : public PlatformTest {
 protected:
  TabStripControllerTest()
      : scene_state_([[SceneState alloc] initWithAppState:nil]) {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
  }

  void SetUp() override {
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET)
      return;

    visible_navigation_item_ = web::NavigationItem::Create();
    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    mock_popup_menu_commands_handler_ =
        OCMStrictProtocolMock(@protocol(PopupMenuCommands));
    mock_application_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationSettingsCommands));

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_settings_commands_handler_
                     forProtocol:@protocol(ApplicationSettingsCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_popup_menu_commands_handler_
                     forProtocol:@protocol(PopupMenuCommands)];

    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

    controller_ = [[TabStripController alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()
                             style:NORMAL
                 layoutGuideCenter:LayoutGuideCenterForBrowser(browser_.get())];
  }

  void TearDown() override {
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET)
      return;
    [controller_ disconnect];
  }

  void AddWebStateForTesting(std::string title) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetTitle(base::UTF8ToUTF16(title));
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetVisibleItem(visible_navigation_item_.get());
    web_state->SetNavigationManager(std::move(navigation_manager));
    browser_->GetWebStateList()->InsertWebState(
        /*index=*/0, std::move(web_state), WebStateList::INSERT_NO_FLAGS,
        WebStateOpener());
  }

  void DetachWebStateForTesting(int index) {
    browser_->GetWebStateList()->DetachWebStateAt(index);
  }

  void ActivateWebStateForTesting(int index) {
    browser_->GetWebStateList()->ActivateWebStateAt(index);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::NavigationItem> visible_navigation_item_;
  id mock_application_commands_handler_;
  id mock_popup_menu_commands_handler_;
  id mock_application_settings_commands_handler_;
  TabStripController* controller_;
  UIWindow* window_;
  SceneState* scene_state_;
};

TEST_F(TabStripControllerTest, LoadAndDisplay) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET)
    return;
  AddWebStateForTesting("Tab Title 1");
  AddWebStateForTesting("Tab Title 2");
  // Force the view to load.
  UIWindow* window = [[UIWindow alloc] initWithFrame:CGRectZero];
  [window addSubview:[controller_ view]];
  window_ = window;

  // There should be two TabViews and one new tab button nested within the
  // parent view (which contains exactly one scroll view).
  EXPECT_EQ(3U,
            [[[[[controller_ view] subviews] objectAtIndex:0] subviews] count]);
}

// Tests the active index when an active WebState is detached.
TEST_F(TabStripControllerTest, DetachActiveWebState) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  AddWebStateForTesting("Tab Title 1");
  AddWebStateForTesting("Tab Title 2");
  AddWebStateForTesting("Tab Title 3");

  ActivateWebStateForTesting(1);
  EXPECT_EQ(1, browser_->GetWebStateList()->active_index());

  // Detaching an active WebState shouldn't change the active index.
  DetachWebStateForTesting(1);
  EXPECT_EQ(1, browser_->GetWebStateList()->active_index());
}

// Tests the active index when a WebState before an active WebState is detached.
TEST_F(TabStripControllerTest, DetachWebStateBeforeActiveWebState) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  AddWebStateForTesting("Tab Title 1");
  AddWebStateForTesting("Tab Title 2");
  AddWebStateForTesting("Tab Title 3");

  ActivateWebStateForTesting(1);
  EXPECT_EQ(1, browser_->GetWebStateList()->active_index());

  // Detaching a WebState before an active WebState decrements the active index.
  DetachWebStateForTesting(0);
  EXPECT_EQ(0, browser_->GetWebStateList()->active_index());
}

// Tests the active index when a WebState after an active WebState is detached.
TEST_F(TabStripControllerTest, DetachWebStateAfterActiveWebState) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  AddWebStateForTesting("Tab Title 1");
  AddWebStateForTesting("Tab Title 2");
  AddWebStateForTesting("Tab Title 3");

  ActivateWebStateForTesting(1);
  EXPECT_EQ(1, browser_->GetWebStateList()->active_index());

  // Detaching a WebState after an active WebState shouldn't change the active
  // index.
  DetachWebStateForTesting(2);
  EXPECT_EQ(1, browser_->GetWebStateList()->active_index());
}

}  // namespace
