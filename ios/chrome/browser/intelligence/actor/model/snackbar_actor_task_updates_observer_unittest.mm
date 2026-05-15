// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/snackbar_actor_task_updates_observer.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace actor {

class SnackbarActorTaskUpdatesObserverTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());

    // Create regular browser.
    regular_browser_ = std::make_unique<TestBrowser>(profile_.get());
    browser_list_->AddBrowser(regular_browser_.get());

    // Set up Mock SnackbarCommands on regular browser.
    mock_snackbar_commands_ = OCMProtocolMock(@protocol(SnackbarCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_commands_
                     forProtocol:@protocol(SnackbarCommands)];

    observer_ = [[SnackbarActorTaskUpdatesObserver alloc]
        initWithProfile:profile_.get()];
  }

  void TearDown() override {
    observer_ = nil;
    [regular_browser_->GetCommandDispatcher()
        stopDispatchingToTarget:mock_snackbar_commands_];
    browser_list_->RemoveBrowser(regular_browser_.get());
    browser_list_ = nullptr;
    regular_browser_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<BrowserList> browser_list_;
  std::unique_ptr<TestBrowser> regular_browser_;
  id mock_snackbar_commands_;
  SnackbarActorTaskUpdatesObserver* observer_;
};

// Test registration callback maps correct formatting for states.
TEST_F(SnackbarActorTaskUpdatesObserverTest, TestRegistrationStateFormatting) {
  actor::ActorTaskId task_id(123);

  // 1. Test Init state
  [[mock_snackbar_commands_ expect]
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                      SnackbarMessage* message) {
        return [message.title isEqualToString:@"Test Task"] &&
               [message.subtitle isEqualToString:@"Starting up"] &&
               [message.secondarySubtitle isEqualToString:@"State: Started"];
      }]];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Starting up"
                               currentState:actor::ActorTaskState::kInit
                                  webStates:@[]];
  [mock_snackbar_commands_ verify];

  // 2. Test Finished state
  [[mock_snackbar_commands_ expect]
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                      SnackbarMessage* message) {
        return [message.title isEqualToString:@"Test Task"] &&
               [message.subtitle isEqualToString:@"No task"] &&
               [message.secondarySubtitle isEqualToString:@"State: Finished"];
      }]];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@""
                               currentState:actor::ActorTaskState::kFinished
                                  webStates:@[]];
  [mock_snackbar_commands_ verify];

  // 3. Test Failed state
  [[mock_snackbar_commands_ expect]
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                      SnackbarMessage* message) {
        return [message.title isEqualToString:@"Test Task"] &&
               [message.subtitle isEqualToString:@"No task"] &&
               [message.secondarySubtitle isEqualToString:@"State: Failed"];
      }]];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@""
                               currentState:actor::ActorTaskState::kFailed
                                  webStates:@[]];
  [mock_snackbar_commands_ verify];

  // 4. Test Paused state
  [[mock_snackbar_commands_ expect]
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                      SnackbarMessage* message) {
        return [message.title isEqualToString:@"Test Task"] &&
               [message.subtitle isEqualToString:@"User stopped"] &&
               [message.secondarySubtitle isEqualToString:@"State: Paused"];
      }]];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"User stopped"
                               currentState:actor::ActorTaskState::kPausedByUser
                                  webStates:@[]];
  [mock_snackbar_commands_ verify];

  // 5. Test Acting state (filtered, should not display snackbar)
  [[mock_snackbar_commands_ reject] showSnackbarMessage:[OCMArg any]];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kActing
                                  webStates:@[]];
  [mock_snackbar_commands_ verify];
}

// Test state change mapping.
TEST_F(SnackbarActorTaskUpdatesObserverTest, TestStateChangeNotification) {
  actor::ActorTaskId task_id(123);

  // Register observer. Acting state should not show a snackbar.
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kActing
                                  webStates:@[]];

  [[mock_snackbar_commands_ expect]
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                      SnackbarMessage* message) {
        return [message.title isEqualToString:@"Test Task"] &&
               [message.subtitle isEqualToString:@"Finding a recipe"] &&
               [message.secondarySubtitle isEqualToString:@"State: Finished"];
      }]];
  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kFinished
                   fromState:actor::ActorTaskState::kActing];
  [mock_snackbar_commands_ verify];

  [[mock_snackbar_commands_ expect]
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                      SnackbarMessage* message) {
        return [message.title isEqualToString:@"Test Task"] &&
               [message.subtitle isEqualToString:@"Finding a recipe"] &&
               [message.secondarySubtitle isEqualToString:@"State: Failed"];
      }]];
  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kFailed
                   fromState:actor::ActorTaskState::kActing];
  [mock_snackbar_commands_ verify];

  // Filtered states should not emit a snackbar message.
  [[mock_snackbar_commands_ reject] showSnackbarMessage:[OCMArg any]];
  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kActing
                   fromState:actor::ActorTaskState::kInit];

  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kPausedByActor
                   fromState:actor::ActorTaskState::kActing];
  [mock_snackbar_commands_ verify];
}

// Test tool execution message.
TEST_F(SnackbarActorTaskUpdatesObserverTest, TestWillExecuteTool) {
  actor::ActorTaskId task_id(123);
  web::WebStateID web_state_id = web::WebStateID::FromSerializedValue(999);

  // Register observer. Acting state should not show a snackbar.
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kActing
                                  webStates:@[]];

  [[mock_snackbar_commands_ expect]
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                      SnackbarMessage* message) {
        return [message.title isEqualToString:@"Test Task"] &&
               [message.subtitle isEqualToString:@"Navigating..."] &&
               [message.secondarySubtitle
                   isEqualToString:@"Executing: NavigateTool"];
      }]];
  [observer_ actorTaskWithID:task_id
             willExecuteTool:@"NavigateTool"
                  taskUpdate:@"Navigating..."
                  onWebState:web_state_id];
  [mock_snackbar_commands_ verify];
}

}  // namespace actor
