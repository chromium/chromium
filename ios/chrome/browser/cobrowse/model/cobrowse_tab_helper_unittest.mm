// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_tab_helper.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class CobrowseTabHelperTest : public PlatformTest {
 protected:
  CobrowseTabHelperTest() {
    feature_list_.InitAndEnableFeature(kAimCobrowse);

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_list_ = browser_->GetWebStateList();

    // Create a mock dispatcher and associate it with a mock scene commands
    // handler.
    mock_command_dispatcher_ = OCMClassMock([CommandDispatcher class]);
    mock_scene_commands_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    OCMStub([mock_command_dispatcher_
                strictCallableForProtocol:@protocol(SceneCommands)])
        .andReturn(mock_scene_commands_handler_);
    browser_->SetCommandDispatcher(mock_command_dispatcher_);

    CobrowseBrowserAgent::CreateForBrowser(browser_.get());

    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    fake_web_state_ = fake_web_state.get();
    CobrowseTabHelper::CreateForWebState(fake_web_state_);
    web_state_list_->InsertWebState(std::move(fake_web_state),
                                    WebStateList::InsertionParams::Automatic());

    tab_helper_ = CobrowseTabHelper::FromWebState(fake_web_state_);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<web::FakeWebState> fake_web_state_;
  raw_ptr<CobrowseTabHelper> tab_helper_;
  id mock_scene_commands_handler_;
  id mock_command_dispatcher_;
};

// Tests that showAssistant is called when navigating in a new tab if the opener
// was an AIM URL.
TEST_F(CobrowseTabHelperTest, TriggerAssistantFromOpener) {
  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL next_url("https://www.example.com");

  // Create an opener WebState and add it to the list.
  auto opener_web_state = std::make_unique<web::FakeWebState>();
  opener_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  opener_web_state->SetCurrentURL(aim_url);
  web::FakeWebState* opener_ptr = opener_web_state.get();
  CobrowseTabHelper::CreateForWebState(opener_web_state.get());
  web_state_list_->InsertWebState(std::move(opener_web_state));

  // Create a new WebState with the opener and add it to the list.
  auto new_web_state = std::make_unique<web::FakeWebState>();
  new_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  new_web_state->SetCurrentURL(GURL::EmptyGURL());
  web::FakeWebState* new_web_state_ptr = new_web_state.get();
  CobrowseTabHelper::CreateForWebState(new_web_state_ptr);
  web_state_list_->InsertWebState(
      std::move(new_web_state),
      WebStateList::InsertionParams::Automatic().WithOpener(
          WebStateOpener(opener_ptr)));

  CobrowseTabHelper* new_tab_helper =
      CobrowseTabHelper::FromWebState(new_web_state_ptr);

  web::FakeNavigationContext context;
  context.SetUrl(next_url);

  OCMExpect([mock_scene_commands_handler_ showAssistant]);

  new_tab_helper->DidStartNavigation(new_web_state_ptr, &context);

  [mock_scene_commands_handler_ verify];
}

// Tests that showAssistant is NOT called when navigating in a new tab if the
// opener was NOT an AIM URL.
TEST_F(CobrowseTabHelperTest, NoTriggerFromNonAimOpener) {
  GURL non_aim_url("https://www.google.com/search?q=test");
  GURL next_url("https://www.example.com");

  // Create an opener WebState and add it to the list.
  auto opener_web_state = std::make_unique<web::FakeWebState>();
  opener_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  opener_web_state->SetCurrentURL(non_aim_url);
  web::FakeWebState* opener_ptr = opener_web_state.get();
  CobrowseTabHelper::CreateForWebState(opener_web_state.get());
  web_state_list_->InsertWebState(std::move(opener_web_state));

  // Create a new WebState with the opener and add it to the list.
  auto new_web_state = std::make_unique<web::FakeWebState>();
  new_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  new_web_state->SetCurrentURL(GURL::EmptyGURL());
  web::FakeWebState* new_web_state_ptr = new_web_state.get();
  CobrowseTabHelper::CreateForWebState(new_web_state_ptr);
  web_state_list_->InsertWebState(
      std::move(new_web_state),
      WebStateList::InsertionParams::Automatic().WithOpener(
          WebStateOpener(opener_ptr)));

  CobrowseTabHelper* new_tab_helper =
      CobrowseTabHelper::FromWebState(new_web_state_ptr);

  web::FakeNavigationContext context;
  context.SetUrl(next_url);

  [[mock_scene_commands_handler_ reject] showAssistant];

  new_tab_helper->DidStartNavigation(new_web_state_ptr, &context);

  [mock_scene_commands_handler_ verify];
}

// Tests that showAssistant is NOT called when navigating in the same tab,
// even if it's an AIM URL, because it doesn't have an opener.
TEST_F(CobrowseTabHelperTest, NoTriggerInSameTab) {
  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL non_aim_url("https://www.google.com/search?q=test");

  fake_web_state_->SetCurrentURL(aim_url);

  web::FakeNavigationContext context;
  context.SetUrl(non_aim_url);

  [[mock_scene_commands_handler_ reject] showAssistant];

  tab_helper_->DidStartNavigation(fake_web_state_, &context);

  [mock_scene_commands_handler_ verify];
}
