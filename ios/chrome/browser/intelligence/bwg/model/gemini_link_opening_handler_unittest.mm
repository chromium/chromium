// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_link_opening_handler.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

namespace {

constexpr char kTestURL[] = "https://www.example.com/";

}  // namespace

// Test fixture for GeminiLinkOpeningHandler.
class GeminiLinkOpeningHandlerTest : public PlatformTest {
 protected:
  GeminiLinkOpeningHandlerTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Create the notifier and inject the fake URL loading browser agent.
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());

    // Get the fake URL loader from the browser.
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    mock_scene_commands_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    mock_gemini_commands_handler_ = OCMProtocolMock(@protocol(GeminiCommands));

    // Initialize the link opening handler with the fake loader and mock
    // command handlers.
    link_opening_handler_ =
        [[GeminiLinkOpeningHandler alloc] initWithURLLoader:url_loader_];
    link_opening_handler_.sceneHandler = mock_scene_commands_handler_;
    link_opening_handler_.geminiHandler = mock_gemini_commands_handler_;
  }

  ~GeminiLinkOpeningHandlerTest() override {
    [link_opening_handler_ disconnect];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_ = nullptr;
  id mock_scene_commands_handler_;
  id mock_gemini_commands_handler_;
  base::UserActionTester user_action_tester_;
  GeminiLinkOpeningHandler* link_opening_handler_;
};

// Tests that when Copresence is enabled, openURLInNewTab uses
// URLLoadingAgent to load the correct URL.
TEST_F(GeminiLinkOpeningHandlerTest, TestOpenURLInNewTabWithCopresence) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{kPageActionMenu},
      /*disabled_features=*/{});

  [link_opening_handler_ openURLInNewTab:@(kTestURL)];

  // Verify the URL was passed to the loader.
  EXPECT_EQ(kTestURL, url_loader_->last_params.web_params.url.spec());
}

// Tests that closePresentedViewsAndOpenURLInNewTab calls the Scene Command
// openURLInNewTab to load the correct URL.
TEST_F(GeminiLinkOpeningHandlerTest, TestClosePresentedViewsAndOpenURL) {
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
TEST_F(GeminiLinkOpeningHandlerTest, TestOpenURLInNewTabRecordsUserAction) {
  [link_opening_handler_ openURLInNewTab:@(kTestURL)];

  // Verify that the URL opened user action was recorded.
  EXPECT_EQ(1, user_action_tester_.GetActionCount("MobileGeminiURLOpened"));
}

// Tests that closePresentedViewsAndOpenURLInNewTab records the appropriate user
// action.
TEST_F(GeminiLinkOpeningHandlerTest,
       TestClosePresentedViewsAndOpenURLRecordsUserAction) {
  [link_opening_handler_ closePresentedViewsAndOpenURLInNewTab:@(kTestURL)];

  // Verify that the URL opened user action was recorded.
  EXPECT_EQ(1, user_action_tester_.GetActionCount("MobileGeminiURLOpened"));
}

// Tests that openURLInNewTab handles empty URL gracefully.
TEST_F(GeminiLinkOpeningHandlerTest, TestOpenURLInNewTabWithEmptyURL) {
  [link_opening_handler_ openURLInNewTab:@""];

  // The URL will be invalid so the loader should not be called.
  EXPECT_FALSE(url_loader_->last_params.web_params.url.is_valid());
  EXPECT_TRUE(url_loader_->last_params.web_params.url.is_empty());
}

// Tests that closePresentedViewsAndOpenURLInNewTab handles empty URL
// gracefully.
TEST_F(GeminiLinkOpeningHandlerTest,
       TestClosePresentedViewsAndOpenURLWithEmptyURL) {
  // Expect that openURLInNewTab is not called.
  [[mock_scene_commands_handler_ reject]
      closePresentedViewsAndOpenURL:[OCMArg any]];

  [link_opening_handler_ closePresentedViewsAndOpenURLInNewTab:@""];

  [mock_scene_commands_handler_ verify];
}

// Tests that opening a URL calls minimizeGeminiIfInvoked on GeminiCommands.
TEST_F(GeminiLinkOpeningHandlerTest, TestOpenURLCallsMinimizeGeminiIfInvoked) {
  OCMExpect([mock_gemini_commands_handler_ minimizeGeminiIfInvoked]);

  [link_opening_handler_ openURLInNewTab:@(kTestURL)];

  [mock_gemini_commands_handler_ verify];
}

// Tests that opening an invalid URL does not call minimizeGeminiIfInvoked.
TEST_F(GeminiLinkOpeningHandlerTest,
       TestOpenInvalidURLDoesNotCallMinimizeGeminiIfInvoked) {
  [[mock_gemini_commands_handler_ reject] minimizeGeminiIfInvoked];

  [link_opening_handler_ openURLInNewTab:@""];

  [mock_gemini_commands_handler_ verify];
}

// Test that openImmersiveLearningCardURLInWebModal: forwards the URL to
// GeminiCommands.
TEST_F(GeminiLinkOpeningHandlerTest,
       TestOpenImmersiveLearningCardURLInWebModal) {
  // OCMock compares C++ reference args by address, so capture the URL instead.
  __block GURL opened_url;
  OCMExpect([mock_gemini_commands_handler_ showGeminiWebModalForURL:GURL()])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation* invocation) {
        const GURL* url = nullptr;
        [invocation getArgument:&url atIndex:2];
        opened_url = *url;
      });

  [link_opening_handler_
      openImmersiveLearningCardURLInWebModal:[NSURL URLWithString:@(kTestURL)]];

  [mock_gemini_commands_handler_ verify];
  EXPECT_EQ(GURL(kTestURL), opened_url);
}

// Test that openImmersiveLearningCardURLInWebModal: records the URL opened and
// web modal opened user actions.
TEST_F(GeminiLinkOpeningHandlerTest,
       TestOpenImmersiveLearningCardURLInWebModalRecordsUserAction) {
  [link_opening_handler_
      openImmersiveLearningCardURLInWebModal:[NSURL URLWithString:@(kTestURL)]];

  EXPECT_EQ(1, user_action_tester_.GetActionCount("MobileGeminiURLOpened"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "MobileGeminiImmersiveLearningCardWebModalOpened"));
}

// Test that openImmersiveLearningCardURLInWebModal: ignores a nil URL.
TEST_F(GeminiLinkOpeningHandlerTest,
       TestOpenImmersiveLearningCardURLInWebModalWithNilURL) {
  [[[mock_gemini_commands_handler_ reject] ignoringNonObjectArgs]
      showGeminiWebModalForURL:GURL()];

  [link_opening_handler_ openImmersiveLearningCardURLInWebModal:nil];

  [mock_gemini_commands_handler_ verify];
  EXPECT_EQ(0, user_action_tester_.GetActionCount("MobileGeminiURLOpened"));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   "MobileGeminiImmersiveLearningCardWebModalOpened"));
}

// Test that openImmersiveLearningCardURLInWebModal: opens the URL in a new tab
// instead of the web modal when the `gemini_quizzes_open_in_new_tab` arm is
// enabled.
TEST_F(GeminiLinkOpeningHandlerTest,
       TestOpenImmersiveLearningCardURLInNewTabWhenParamEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kGeminiQuizzes, {{kGeminiQuizzesOpenInNewTabParam, "true"}});
  [[[mock_gemini_commands_handler_ reject] ignoringNonObjectArgs]
      showGeminiWebModalForURL:GURL()];
  OCMExpect([mock_gemini_commands_handler_ minimizeGeminiIfInvoked]);

  [link_opening_handler_
      openImmersiveLearningCardURLInWebModal:[NSURL URLWithString:@(kTestURL)]];

  [mock_gemini_commands_handler_ verify];
  EXPECT_EQ(GURL(kTestURL), url_loader_->last_params.web_params.url);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("MobileGeminiURLOpened"));
}

// Test that openImmersiveLearningCardURLInWebModal: does not open a new tab
// when the `gemini_quizzes_open_in_new_tab` arm is disabled.
TEST_F(GeminiLinkOpeningHandlerTest,
       TestOpenImmersiveLearningCardURLDoesNotOpenNewTabByDefault) {
  base::test::ScopedFeatureList scoped_feature_list(kGeminiQuizzes);

  [link_opening_handler_
      openImmersiveLearningCardURLInWebModal:[NSURL URLWithString:@(kTestURL)]];

  EXPECT_TRUE(url_loader_->last_params.web_params.url.is_empty());
}

// Tests that calling `disconnect` clears handlers and prevents subsequent URL
// loads, command dispatches, and user action recordings.
TEST_F(GeminiLinkOpeningHandlerTest, TestDisconnect) {
  [link_opening_handler_ disconnect];

  [[mock_scene_commands_handler_ reject]
      closePresentedViewsAndOpenURL:[OCMArg any]];
  [[mock_gemini_commands_handler_ reject] minimizeGeminiIfInvoked];
  [[[mock_gemini_commands_handler_ reject] ignoringNonObjectArgs]
      showGeminiWebModalForURL:GURL()];

  [link_opening_handler_ openURLInNewTab:@(kTestURL)];
  [link_opening_handler_ closePresentedViewsAndOpenURLInNewTab:@(kTestURL)];
  [link_opening_handler_
      openImmersiveLearningCardURLInWebModal:[NSURL URLWithString:@(kTestURL)]];

  EXPECT_TRUE(url_loader_->last_params.web_params.url.is_empty());
  EXPECT_EQ(0, user_action_tester_.GetActionCount("MobileGeminiURLOpened"));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   "MobileGeminiImmersiveLearningCardWebModalOpened"));
  [mock_scene_commands_handler_ verify];
  [mock_gemini_commands_handler_ verify];
}
