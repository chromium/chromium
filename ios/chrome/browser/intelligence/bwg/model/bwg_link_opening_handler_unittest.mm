// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_handler.h"

#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

constexpr char kTestURL[] = "https://www.example.com/";

}  // namespace

// Test fixture for BWGLinkOpeningHandler.
class BwgLinkOpeningHandlerTest : public PlatformTest {
 protected:
  BwgLinkOpeningHandlerTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Create the notifier and inject the fake URL loading browser agent.
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());

    // Get the fake URL loader from the browser.
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    // Create a mock dispatcher and associate it with a mock scene commands
    // handler.
    mock_dispatcher_ = OCMClassMock([CommandDispatcher class]);
    mock_scene_commands_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    OCMStub(
        [mock_dispatcher_ strictCallableForProtocol:@protocol(SceneCommands)])
        .andReturn(mock_scene_commands_handler_);

    // Initialize the link opening handler with the fake loader and mock
    // dispatcher.
    link_opening_handler_ =
        [[BWGLinkOpeningHandler alloc] initWithURLLoader:url_loader_
                                              dispatcher:mock_dispatcher_];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  id mock_dispatcher_;
  id mock_scene_commands_handler_;
  base::UserActionTester user_action_tester_;
  BWGLinkOpeningHandler* link_opening_handler_;
};

// Tests that when Copresence is enabled, openURLInNewTab uses
// URLLoadingAgent to load the correct URL.
TEST_F(BwgLinkOpeningHandlerTest, TestOpenURLInNewTabWithCopresence) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGeminiCopresence);

  [link_opening_handler_ openURLInNewTab:@(kTestURL)];

  // Verify the URL was passed to the loader.
  EXPECT_EQ(kTestURL, url_loader_->last_params.web_params.url.spec());
}

// Tests that when Copresence is disabled, openURLInNewTab uses the Scene
// Command openURLInNewTab to load the correct URL.
TEST_F(BwgLinkOpeningHandlerTest, TestOpenURLInNewTabWithoutCopresence) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kGeminiCopresence);

  // Expect that openURLInNewTab is called with the correct URL.
  OCMExpect([mock_scene_commands_handler_
      closePresentedViewsAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                                OpenNewTabCommand* command) {
        return command.URL.spec() == kTestURL;
      }]]);

  [link_opening_handler_ openURLInNewTab:@(kTestURL)];

  [mock_scene_commands_handler_ verify];
}

// Tests that closePresentedViewsAndOpenURLInNewTab calls the Scene Command
// openURLInNewTab to load the correct URL.
TEST_F(BwgLinkOpeningHandlerTest, TestClosePresentedViewsAndOpenURL) {
  // Expect that openURLInNewTab is called with the correct URL.
  OCMExpect([mock_scene_commands_handler_
      closePresentedViewsAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                                OpenNewTabCommand* command) {
        return command.URL.spec() == kTestURL;
      }]]);

  [link_opening_handler_ closePresentedViewsAndOpenURLInNewTab:@(kTestURL)];

  [mock_scene_commands_handler_ verify];
}

// Tests that openURLInNewTab records the appropriate user action.
TEST_F(BwgLinkOpeningHandlerTest, TestOpenURLInNewTabRecordsUserAction) {
  [link_opening_handler_ openURLInNewTab:@(kTestURL)];

  // Verify that the URL opened user action was recorded.
  EXPECT_EQ(1, user_action_tester_.GetActionCount("MobileGeminiURLOpened"));
}

// Tests that closePresentedViewsAndOpenURLInNewTab records the appropriate user
// action.
TEST_F(BwgLinkOpeningHandlerTest,
       TestClosePresentedViewsAndOpenURLRecordsUserAction) {
  [link_opening_handler_ closePresentedViewsAndOpenURLInNewTab:@(kTestURL)];

  // Verify that the URL opened user action was recorded.
  EXPECT_EQ(1, user_action_tester_.GetActionCount("MobileGeminiURLOpened"));
}

// Tests that openURLInNewTab handles empty URL gracefully.
TEST_F(BwgLinkOpeningHandlerTest, TestOpenURLInNewTabWithEmptyURL) {
  [link_opening_handler_ openURLInNewTab:@""];

  // The URL will be invalid so the loader should not be called.
  EXPECT_FALSE(url_loader_->last_params.web_params.url.is_valid());
  EXPECT_TRUE(url_loader_->last_params.web_params.url.is_empty());
}

// Tests that closePresentedViewsAndOpenURLInNewTab handles empty URL
// gracefully.
TEST_F(BwgLinkOpeningHandlerTest,
       TestClosePresentedViewsAndOpenURLWithEmptyURL) {
  // Expect that openURLInNewTab is not called.
  [[mock_scene_commands_handler_ reject]
      closePresentedViewsAndOpenURL:[OCMArg any]];

  [link_opening_handler_ closePresentedViewsAndOpenURLInNewTab:@""];

  [mock_scene_commands_handler_ verify];
}
