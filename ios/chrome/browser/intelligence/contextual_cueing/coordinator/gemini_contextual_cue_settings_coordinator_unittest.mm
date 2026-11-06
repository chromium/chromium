// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/coordinator/gemini_contextual_cue_settings_coordinator.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_mutator.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class GeminiContextualCueSettingsCoordinatorTest : public PlatformTest {
 protected:
  GeminiContextualCueSettingsCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    gemini_handler_ = OCMStrictProtocolMock(@protocol(GeminiCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:gemini_handler_
                     forProtocol:@protocol(GeminiCommands)];

    settings_handler_ = OCMStrictProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:settings_handler_
                     forProtocol:@protocol(SettingsCommands)];

    base_view_controller_ = OCMPartialMock([[UIViewController alloc] init]);
    coordinator_ = [[GeminiContextualCueSettingsCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
  }

  ~GeminiContextualCueSettingsCoordinatorTest() override {
    [coordinator_ stop];
  }

  // Starts `coordinator_` and returns the presented
  // `GeminiContextualCueSettingsViewController`.
  GeminiContextualCueSettingsViewController* StartAndGetViewController() {
    __block UINavigationController* presented_nav = nil;
    OCMExpect([base_view_controller_
        presentViewController:[OCMArg checkWithBlock:^BOOL(id vc) {
          presented_nav = base::apple::ObjCCast<UINavigationController>(vc);
          return presented_nav != nil;
        }]
                     animated:YES
                   completion:nil]);
    [coordinator_ start];
    return base::apple::ObjCCast<GeminiContextualCueSettingsViewController>(
        presented_nav.topViewController);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id gemini_handler_ = nil;
  id settings_handler_ = nil;
  id base_view_controller_ = nil;
  GeminiContextualCueSettingsCoordinator* coordinator_ = nil;
};

// Tests that tapping Done dispatches `hideGeminiContextualCueSettingsModal`.
TEST_F(GeminiContextualCueSettingsCoordinatorTest,
       TapDoneHidesContextualCueSettingsModal) {
  GeminiContextualCueSettingsViewController* view_controller =
      StartAndGetViewController();
  ASSERT_TRUE(view_controller);

  OCMExpect([gemini_handler_ hideGeminiContextualCueSettingsModal]);

  [view_controller.presentationDelegate
      geminiContextualCueSettingsViewControllerDidTapDone:view_controller];

  EXPECT_OCMOCK_VERIFY(gemini_handler_);
  EXPECT_OCMOCK_VERIFY(base_view_controller_);
}

// Tests that selecting "Manage Gemini suggestions" via the mutator hides the
// modal and dispatches `showGeminiContextualCueSettings`.
TEST_F(GeminiContextualCueSettingsCoordinatorTest,
       DidTapManageSuggestionsShowsGeminiSuggestionsSettings) {
  GeminiContextualCueSettingsViewController* view_controller =
      StartAndGetViewController();
  ASSERT_TRUE(view_controller);
  ASSERT_TRUE(view_controller.mutator);

  OCMExpect([gemini_handler_ hideGeminiContextualCueSettingsModal]);
  OCMExpect([settings_handler_ showGeminiContextualCueSettings]);

  [view_controller.mutator didTapManageSuggestions];

  EXPECT_OCMOCK_VERIFY(gemini_handler_);
  EXPECT_OCMOCK_VERIFY(settings_handler_);
  EXPECT_OCMOCK_VERIFY(base_view_controller_);
}

// Tests that selecting "Edit prompt" via the mutator hides the modal and
// starts the Gemini entry flow with `shouldAutoSubmit = NO`.
TEST_F(GeminiContextualCueSettingsCoordinatorTest,
       DidTapEditPromptStartsGeminiEntryFlow) {
  GeminiContextualCueSettingsViewController* view_controller =
      StartAndGetViewController();
  ASSERT_TRUE(view_controller);
  ASSERT_TRUE(view_controller.mutator);

  OCMExpect([gemini_handler_ hideGeminiContextualCueSettingsModal]);
  OCMExpect([gemini_handler_
      startGeminiEntryFlowWithStartupState:[OCMArg checkWithBlock:^BOOL(
                                                       GeminiStartupState*
                                                           state) {
        return state.entryPoint == gemini::EntryPoint::ContextualCueInfobar &&
               !state.shouldAutoSubmit;
      }]
                        baseViewController:nil
                  showSnackbarOnCompletion:YES
                                completion:nil]);

  [view_controller.mutator didTapEditPrompt];

  EXPECT_OCMOCK_VERIFY(gemini_handler_);
  EXPECT_OCMOCK_VERIFY(base_view_controller_);
}
