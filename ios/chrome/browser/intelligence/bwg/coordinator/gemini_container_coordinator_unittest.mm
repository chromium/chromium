// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_coordinator.h"

#import <memory>

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface GeminiContainerCoordinator (Testing)
- (void)dismissGeminiFromOtherWindowsWithCompletion:(ProceduralBlock)completion;
@end

class GeminiContainerCoordinatorTest : public PlatformTest {
 public:
  GeminiContainerCoordinatorTest() {
    feature_list_.InitWithFeatures(
        {kAssistantContainer, kIOSGeminiBottomSheetMigration}, {});

    TestProfileIOS::Builder builder;
    builder.SetName("profile1");
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    browser_ = std::make_unique<TestBrowser>(profile_);
    BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
    browser_list->AddBrowser(browser_.get());

    mock_gemini_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_gemini_handler_
                             forProtocol:@protocol(GeminiCommands)];
    [dispatcher startDispatchingToTarget:mock_settings_handler_
                             forProtocol:@protocol(SettingsCommands)];

    base_view_controller_ = [[UIViewController alloc] init];
    startup_state_ = [[GeminiStartupState alloc] init];

    coordinator_ = [[GeminiContainerCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                      startupState:startup_state_];
  }

  ~GeminiContainerCoordinatorTest() override {
    [coordinator_ stop];
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher stopDispatchingForProtocol:@protocol(GeminiCommands)];
    [dispatcher stopDispatchingForProtocol:@protocol(SettingsCommands)];
    BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
    browser_list->RemoveBrowser(browser_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  id mock_gemini_handler_;
  id mock_settings_handler_;
  UIViewController* base_view_controller_;
  GeminiStartupState* startup_state_;
  GeminiContainerCoordinator* coordinator_;
};

// Test that `dismissGeminiFromOtherWindowsWithCompletion:` executes completion
// when no other browsers exist.
TEST_F(GeminiContainerCoordinatorTest,
       TestDismissGeminiFromOtherWindowsWhenNoOtherBrowsers) {
  base::test::TestFuture<void> future;
  [coordinator_
      dismissGeminiFromOtherWindowsWithCompletion:base::CallbackToBlock(
                                                      future.GetCallback())];
  EXPECT_TRUE(future.Wait());
}

// Test that `dismissGeminiFromOtherWindowsWithCompletion:` dismisses Gemini in
// other browsers.
TEST_F(GeminiContainerCoordinatorTest, TestDismissGeminiFromOtherWindows) {
  TestProfileIOS::Builder second_profile_builder;
  second_profile_builder.SetName("profile2");
  TestProfileIOS* second_profile =
      profile_manager_.AddProfileWithBuilder(std::move(second_profile_builder));

  // Emulate opening a new window.
  std::unique_ptr<TestBrowser> second_browser =
      std::make_unique<TestBrowser>(second_profile);
  BrowserList* second_browser_list =
      BrowserListFactory::GetForProfile(second_profile);
  second_browser_list->AddBrowser(second_browser.get());

  id mock_second_handler = OCMProtocolMock(@protocol(GeminiCommands));
  [second_browser->GetCommandDispatcher()
      startDispatchingToTarget:mock_second_handler
                   forProtocol:@protocol(GeminiCommands)];

  [[mock_second_handler expect]
      dismissGeminiFlowWithCompletion:[OCMArg checkWithBlock:^BOOL(
                                                  ProceduralBlock block) {
        CHECK(block);
        block();
        return YES;
      }]];

  base::test::TestFuture<void> future;
  [coordinator_
      dismissGeminiFromOtherWindowsWithCompletion:base::CallbackToBlock(
                                                      future.GetCallback())];
  EXPECT_TRUE(future.Wait());
  EXPECT_OCMOCK_VERIFY(mock_second_handler);

  [second_browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(GeminiCommands)];
  second_browser_list->RemoveBrowser(second_browser.get());
}

// Test that `start` waits for `dismissGeminiFromOtherWindowsWithCompletion:` to
// finish before presenting the assistant container.
TEST_F(GeminiContainerCoordinatorTest,
       TestStartWaitsForDismissalFromOtherWindowsBeforePresenting) {
  id mock_container_handler =
      OCMProtocolMock(@protocol(AssistantContainerCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_container_handler
                   forProtocol:@protocol(AssistantContainerCommands)];

  TestProfileIOS::Builder second_profile_builder;
  second_profile_builder.SetName("profile2");
  TestProfileIOS* second_profile =
      profile_manager_.AddProfileWithBuilder(std::move(second_profile_builder));

  std::unique_ptr<TestBrowser> second_browser =
      std::make_unique<TestBrowser>(second_profile);
  BrowserList* second_browser_list =
      BrowserListFactory::GetForProfile(second_profile);
  second_browser_list->AddBrowser(second_browser.get());

  id mock_second_gemini_handler = OCMProtocolMock(@protocol(GeminiCommands));
  [second_browser->GetCommandDispatcher()
      startDispatchingToTarget:mock_second_gemini_handler
                   forProtocol:@protocol(GeminiCommands)];

  __block ProceduralBlock captured_dismiss_completion = nil;
  [[mock_second_gemini_handler expect]
      dismissGeminiFlowWithCompletion:[OCMArg checkWithBlock:^BOOL(
                                                  ProceduralBlock block) {
        captured_dismiss_completion = [block copy];
        return YES;
      }]];

  __block BOOL container_presented = NO;
  OCMStub([mock_container_handler
              showAssistantContainerWithContent:[OCMArg any]
                                       delegate:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        container_presented = YES;
      });

  [coordinator_ start];

  EXPECT_OCMOCK_VERIFY(mock_second_gemini_handler);
  EXPECT_FALSE(container_presented);
  ASSERT_TRUE(captured_dismiss_completion != nil);

  captured_dismiss_completion();

  EXPECT_TRUE(container_presented);

  [second_browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(GeminiCommands)];
  [browser_->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(AssistantContainerCommands)];
  second_browser_list->RemoveBrowser(second_browser.get());
}
