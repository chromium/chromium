// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/snackbar_actor_task_updates_observer.h"

#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
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

    // Set up Mock GeminiActorSnackbarCommands on regular browser.
    mock_gemini_snackbar_commands_ =
        OCMProtocolMock(@protocol(GeminiActorSnackbarCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_gemini_snackbar_commands_
                     forProtocol:@protocol(GeminiActorSnackbarCommands)];

    observer_ = [[SnackbarActorTaskUpdatesObserver alloc]
        initWithProfile:profile_.get()];
  }

  void TearDown() override {
    observer_ = nil;
    [regular_browser_->GetCommandDispatcher()
        stopDispatchingToTarget:mock_gemini_snackbar_commands_];
    browser_list_->RemoveBrowser(regular_browser_.get());
    browser_list_ = nullptr;
    regular_browser_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  // Wait for all currently queued or posted asynchronous tasks on the default
  // task runner to finish executing. Since `base::SequencedTaskRunner` executes
  // posted tasks in a strict first-in, first-out order, posting a task to quit
  // a run loop guarantees that all previously posted tasks (such as deferred
  // snackbar presentation tasks) have completed before the loop exits.
  void WaitForTasksToComplete() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<BrowserList> browser_list_;
  std::unique_ptr<TestBrowser> regular_browser_;
  id mock_gemini_snackbar_commands_;
  SnackbarActorTaskUpdatesObserver* observer_;
};

// Test registration callback maps correct formatting for states.
TEST_F(SnackbarActorTaskUpdatesObserverTest, TestRegistrationStateFormatting) {
  actor::ActorTaskId task_id(123);

  // 1. Test Init state (should show task title, task update, and state
  // subtitle)
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Test Task"] &&
               [message.subtitle isEqualToString:@"Starting up"] &&
               [message.secondarySubtitle isEqualToString:@"Started"] &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Starting up"
                               currentState:actor::ActorTaskState::kInit
                                  webStates:@[]];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // 2. Test Finished state (with empty update)
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Finished"] &&
               message.subtitle == nil && message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@""
                               currentState:actor::ActorTaskState::kFinished
                                  webStates:@[]];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // 3. Test Failed state (with empty update)
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Failed"] &&
               message.subtitle == nil && message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@""
                               currentState:actor::ActorTaskState::kFailed
                                  webStates:@[]];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // 4. Test Paused state (should show task update and state subtitle)
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"User stopped"] &&
               [message.subtitle isEqualToString:@"Paused"] &&
               message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"User stopped"
                               currentState:actor::ActorTaskState::kPausedByUser
                                  webStates:@[]];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // 5. Test Cancelled state (should show task update and state subtitle)
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Cancelled by system"] &&
               [message.subtitle isEqualToString:@"Cancelled"] &&
               message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Cancelled by system"
                               currentState:actor::ActorTaskState::kCancelled
                                  webStates:@[]];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // 6. Test Waiting for User state (should show task update and state subtitle)
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Need user input"] &&
               [message.subtitle isEqualToString:@"Waiting for user"] &&
               message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_
      didRegisterAsObserverForTaskID:task_id
                           taskTitle:@"Test Task"
                          taskUpdate:@"Need user input"
                        currentState:actor::ActorTaskState::kWaitingOnUser
                           webStates:@[]];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // 7. Test Acting state (filtered, should not display snackbar)
  [[mock_gemini_snackbar_commands_ reject]
      showGeminiActorSnackbarMessage:[OCMArg any]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kActing
                                  webStates:@[]];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
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

  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Finished"] &&
               message.subtitle == nil && message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kFinished
                   fromState:actor::ActorTaskState::kActing];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Failed"] &&
               message.subtitle == nil && message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kFailed
                   fromState:actor::ActorTaskState::kActing];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // Filtered states should not emit a snackbar message.
  [[mock_gemini_snackbar_commands_ reject]
      showGeminiActorSnackbarMessage:[OCMArg any]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kActing
                   fromState:actor::ActorTaskState::kInit];

  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kPausedByActor
                   fromState:actor::ActorTaskState::kActing];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
}

// Test that transitioning to Reflecting state does not show a snackbar
// immediately, but instead shows it once the preceding snackbar completes.
TEST_F(SnackbarActorTaskUpdatesObserverTest,
       TestReflectingStateDeferredSnackbar) {
  actor::ActorTaskId task_id(123);
  web::WebStateID web_state_id = web::WebStateID::FromSerializedValue(999);

  // 1. Register observer in Acting state.
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kActing
                                  webStates:@[]];

  // 2. Trigger a tool execution, capturing the scheduled snackbar message.
  __block SnackbarMessage* captured_tool_message = nil;
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        captured_tool_message = message;
        return [message.title isEqualToString:@"Navigating update..."] &&
               [message.subtitle isEqualToString:@"Navigating"] &&
               message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];

  [observer_ actorTaskWithID:task_id
             willExecuteTool:actor::ToolType::kNavigate
                  taskUpdate:@"Navigating update..."
                  onWebState:web_state_id];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
  ASSERT_NSNE(nil, captured_tool_message);
  ASSERT_TRUE(captured_tool_message.completionHandler != nil);

  // 3. Transition state to Reflecting. This should NOT immediately show a
  // snackbar.
  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kReflecting
                   fromState:actor::ActorTaskState::kActing];

  // 4. Complete/dismiss the tool execution snackbar. This should now trigger
  // the Reflecting snackbar.
  __block SnackbarMessage* captured_reflecting_message = nil;
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        captured_reflecting_message = message;
        return [message.title isEqualToString:@"Reflecting"] &&
               message.subtitle == nil && message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];

  captured_tool_message.completionHandler(YES);
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
  ASSERT_NSNE(nil, captured_reflecting_message);
  ASSERT_TRUE(captured_reflecting_message.completionHandler != nil);
}

// Test that transitioning to Reflecting state shows a snackbar immediately
// if there is no preceding snackbar currently showing.
TEST_F(SnackbarActorTaskUpdatesObserverTest,
       TestReflectingStateImmediateSnackbar) {
  actor::ActorTaskId task_id(123);

  // 1. Register observer in Acting state. Since Acting state returns nil for
  // display state, no snackbar is shown.
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kActing
                                  webStates:@[]];
  WaitForTasksToComplete();

  // 2. Transition state to Reflecting. Since no snackbar is active, this
  // should immediately show the Reflecting snackbar.
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Reflecting"] &&
               message.subtitle == nil && message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];

  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kReflecting
                   fromState:actor::ActorTaskState::kActing];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
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

  // 1. Test that kNavigate shows a snackbar.
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Navigating update..."] &&
               [message.subtitle isEqualToString:@"Navigating"] &&
               message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ actorTaskWithID:task_id
             willExecuteTool:actor::ToolType::kNavigate
                  taskUpdate:@"Navigating update..."
                  onWebState:web_state_id];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // 2. Test that kWait (non-zero duration) shows a snackbar.
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Waiting update..."] &&
               [message.subtitle isEqualToString:@"Waiting"] &&
               message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ actorTaskWithID:task_id
             willExecuteTool:actor::ToolType::kWait
                  taskUpdate:@"Waiting update..."
                  onWebState:web_state_id];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // 3. Test that kWaitZeroDuration is ignored and does NOT show a snackbar.
  [[mock_gemini_snackbar_commands_ reject]
      showGeminiActorSnackbarMessage:[OCMArg any]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ actorTaskWithID:task_id
             willExecuteTool:actor::ToolType::kWaitZeroDuration
                  taskUpdate:@"Waiting (zero duration)..."
                  onWebState:web_state_id];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
}

// Test that duplicate task updates (even with different string pointer
// instances) are filtered out, and only the new/changed ones are displayed.
TEST_F(SnackbarActorTaskUpdatesObserverTest,
       TestDuplicateTaskUpdatesAreFiltered) {
  actor::ActorTaskId task_id(123);
  web::WebStateID web_state_id = web::WebStateID::FromSerializedValue(999);

  // Register observer. Acting state should not show a snackbar.
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kActing
                                  webStates:@[]];

  // 1. First tool execution with a new task update. It should show both the
  // update and the tool name.
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Navigating update..."] &&
               [message.subtitle isEqualToString:@"Navigating"] &&
               message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ actorTaskWithID:task_id
             willExecuteTool:actor::ToolType::kNavigate
                  taskUpdate:@"Navigating update..."
                  onWebState:web_state_id];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // 2. Second tool execution with the EXACT SAME task update (using a
  // dynamically constructed string to ensure it has a different pointer but
  // identical contents). The duplicate task update should be ignored, so the
  // snackbar title is just the tool display name.
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Waiting"] &&
               message.subtitle == nil && message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];

  NSString* duplicateUpdate =
      [NSString stringWithFormat:@"%@ %@", @"Navigating", @"update..."];
  [observer_ actorTaskWithID:task_id
             willExecuteTool:actor::ToolType::kWait
                  taskUpdate:duplicateUpdate
                  onWebState:web_state_id];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];

  // 3. Third tool execution with a DIFFERENT task update. It should be shown
  // again.
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"New update!"] &&
               [message.subtitle isEqualToString:@"Navigating"] &&
               message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ actorTaskWithID:task_id
             willExecuteTool:actor::ToolType::kNavigate
                  taskUpdate:@"New update!"
                  onWebState:web_state_id];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
}

// Test that if a Reflecting snackbar is currently showing and a tool execution
// starts, the tool snackbar is correctly shown and not dismissed or blocked by
// the reflecting state.
TEST_F(SnackbarActorTaskUpdatesObserverTest,
       TestToolSucceedsAfterReflectingStateSnackbar) {
  actor::ActorTaskId task_id(123);
  web::WebStateID web_state_id = web::WebStateID::FromSerializedValue(999);

  // 1. Register observer in Acting state. Since Acting state returns nil for
  // display state, no snackbar is shown.
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kActing
                                  webStates:@[]];
  WaitForTasksToComplete();

  // 2. Transition state to Reflecting. Since no snackbar is active, this
  // should immediately show the Reflecting snackbar.
  __block SnackbarMessage* captured_reflecting_message = nil;
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        captured_reflecting_message = message;
        return [message.title isEqualToString:@"Reflecting"] &&
               message.subtitle == nil && message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];

  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kReflecting
                   fromState:actor::ActorTaskState::kActing];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
  ASSERT_NSNE(nil, captured_reflecting_message);

  // 3. Now, trigger a tool execution while the reflecting snackbar is showing.
  // This should dismiss the reflecting snackbar and show the tool snackbar.
  __block SnackbarMessage* captured_tool_message = nil;
  [[[mock_gemini_snackbar_commands_ expect] andDo:^(NSInvocation* invocation) {
    // When the tool snackbar is presented, the coordinator dismisses the active
    // reflecting snackbar.
    captured_reflecting_message.completionHandler(NO);
  }]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        captured_tool_message = message;
        return [message.title isEqualToString:@"Navigating update..."] &&
               [message.subtitle isEqualToString:@"Navigating"] &&
               message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];

  [observer_ actorTaskWithID:task_id
             willExecuteTool:actor::ToolType::kNavigate
                  taskUpdate:@"Navigating update..."
                  onWebState:web_state_id];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
  ASSERT_NSNE(nil, captured_tool_message);

  // 4. Now transition to reflecting again or check that a subsequent
  // didChangeState to Reflecting does NOT immediately post another reflecting
  // snackbar because the tool snackbar is still active.
  [[mock_gemini_snackbar_commands_ reject]
      showGeminiActorSnackbarMessage:[OCMArg any]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
  [observer_ actorTaskWithID:task_id
              didChangeState:actor::ActorTaskState::kReflecting
                   fromState:actor::ActorTaskState::kActing];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
}

// Test that activeSnackbarUpdate remains nil if the snackbar handler becomes
// nil before the asynchronous snackbar presentation task runs.
TEST_F(SnackbarActorTaskUpdatesObserverTest,
       TestShowingStateResetsWhenHandlerBecomesNil) {
  actor::ActorTaskId task_id(123);
  web::WebStateID web_state_id = web::WebStateID::FromSerializedValue(999);

  // 1. Register observer in Acting state.
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kActing
                                  webStates:@[]];

  // 2. Trigger a tool execution snackbar, which posts a presentation task.
  [observer_ actorTaskWithID:task_id
             willExecuteTool:actor::ToolType::kNavigate
                  taskUpdate:@"Navigating update..."
                  onWebState:web_state_id];

  // 3. Clear the weak handler pointer to simulate it becoming nil.
  [observer_ setValue:nil forKey:@"geminiSnackbarHandler"];

  // 4. Run the posted presentation task. It should detect the nil handler
  // and return early without setting activeSnackbarUpdate.
  WaitForTasksToComplete();

  EXPECT_TRUE([observer_ valueForKey:@"activeSnackbarUpdate"] == nil);
}

// Test that didRegisterAsObserverForTaskID with a kInit snackbar
// and a taskUpdate and title shows all three in the snackbar.
TEST_F(SnackbarActorTaskUpdatesObserverTest,
       TestRegisterAsObserverWithInitStateAndTaskUpdateAndTitle) {
  actor::ActorTaskId task_id(123);

  __block SnackbarMessage* captured_message = nil;
  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        captured_message = message;
        return [message.title isEqualToString:@"Test Task"] &&
               [message.subtitle isEqualToString:@"Finding a recipe"] &&
               [message.secondarySubtitle isEqualToString:@"Started"] &&
               message.leadingAccessoryImage != nil;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];

  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kInit
                                  webStates:@[]];

  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
  ASSERT_NSNE(nil, captured_message);
}

// Test that actorTaskDidStopWithID displays a "Finished" snackbar.
TEST_F(SnackbarActorTaskUpdatesObserverTest, TestActorTaskDidStop) {
  actor::ActorTaskId task_id(123);

  // Register observer. Acting state should not show a snackbar.
  [observer_ didRegisterAsObserverForTaskID:task_id
                                  taskTitle:@"Test Task"
                                 taskUpdate:@"Finding a recipe"
                               currentState:actor::ActorTaskState::kActing
                                  webStates:@[]];

  [[mock_gemini_snackbar_commands_ expect]
      showGeminiActorSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                                 SnackbarMessage* message) {
        return [message.title isEqualToString:@"Finished"] &&
               message.subtitle == nil && message.secondarySubtitle == nil &&
               message.leadingAccessoryImage != nil && message.duration == 4.0;
      }]
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];

  [observer_ actorTaskDidStopWithID:task_id
                         finalState:actor::ActorTaskState::kFinished];
  WaitForTasksToComplete();
  [mock_gemini_snackbar_commands_ verify];
}

}  // namespace actor
