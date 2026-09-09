// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_app_agent.h"

#import <BackgroundTasks/BackgroundTasks.h>
#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/app/background_task/features.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

NSString* const kTestTaskId = @"test.task.FooBar";
NSString* const kTestTaskTitle = @"Foo";
NSString* const kTestTaskSubtitle = @"Bar";

}  // namespace

// Helper object to dynamically configure the mock app state's active scene.
@interface DynamicMockAppStateHelper : NSObject
@property(nonatomic, strong) id foregroundActiveScene;
@end

@implementation DynamicMockAppStateHelper
@end

#pragma mark - BackgroundContinuedProcessingAppAgentTest

class BackgroundContinuedProcessingAppAgentTest : public PlatformTest {
 public:
  BackgroundContinuedProcessingAppAgentTest() {
    mock_app_state_ = OCMClassMock([AppState class]);
    mock_scene_state_ = OCMClassMock([SceneState class]);
    app_state_helper_ = [[DynamicMockAppStateHelper alloc] init];
    app_state_helper_.foregroundActiveScene = mock_scene_state_;

    DynamicMockAppStateHelper* helper = app_state_helper_;
    OCMStub([mock_app_state_ foregroundActiveScene])
        .andDo(^(NSInvocation* inv) {
          id scene = helper.foregroundActiveScene;
          [inv setReturnValue:&scene];
        });
    OCMStub([mock_app_state_ addObserver:[OCMArg any]]);
    OCMStub([mock_app_state_ removeObserver:[OCMArg any]]);

    mock_scheduler_ = OCMStrictClassMock([BGTaskScheduler class]);
    OCMStub([mock_scheduler_ sharedScheduler]).andReturn(mock_scheduler_);

    agent_ = [[BackgroundContinuedProcessingAppAgent alloc] init];
    agent_.appState = mock_app_state_;
  }

  ~BackgroundContinuedProcessingAppAgentTest() override {
    [mock_app_state_ stopMocking];
    [mock_scene_state_ stopMocking];
    [mock_scheduler_ stopMocking];
  }

  // Creates a test task configuration with default properties.
  BackgroundContinuedProcessingTaskConfiguration* CreateTestConfiguration(
      ProceduralBlock expiration_handler = ^{
      }) {
    DCHECK(expiration_handler);
    BackgroundContinuedProcessingTaskConfiguration* config =
        [[BackgroundContinuedProcessingTaskConfiguration alloc]
                initWithTitle:kTestTaskTitle
            expirationHandler:expiration_handler];
    config.subtitle = kTestTaskSubtitle;
    return config;
  }

  // Stubs task registration and submission in `BGTaskScheduler`. If
  // `out_launch_handler` is provided, captures the registered launch handler.
  void StubScheduler(void (^__strong* out_launch_handler)(BGTask*) = nullptr,
                     BOOL registration_success = YES,
                     BOOL submission_success = YES,
                     NSError* submission_error = nil) {
    if (out_launch_handler) {
      OCMStub([mock_scheduler_
                  registerForTaskWithIdentifier:[OCMArg any]
                                     usingQueue:dispatch_get_main_queue()
                                  launchHandler:[OCMArg checkWithBlock:^BOOL(
                                                            id value) {
                                    DCHECK(value);
                                    *out_launch_handler = [value copy];
                                    return YES;
                                  }]])
          .andReturn(registration_success);
    } else {
      OCMStub([mock_scheduler_
                  registerForTaskWithIdentifier:[OCMArg any]
                                     usingQueue:dispatch_get_main_queue()
                                  launchHandler:[OCMArg any]])
          .andReturn(registration_success);
    }

    OCMStub([mock_scheduler_ submitTaskRequest:[OCMArg any]
                                         error:[OCMArg setTo:submission_error]])
        .andReturn(submission_success);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  id mock_app_state_;
  id mock_scene_state_;
  DynamicMockAppStateHelper* app_state_helper_;
  id mock_scheduler_;
  BackgroundContinuedProcessingAppAgent* agent_;
};

// Tests that requesting a task returns nil when the feature flag is disabled.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestRequestTaskFailsWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnableBackgroundContinuedProcessing);

  EXPECT_EQ([agent_ requestTaskWithIdentifier:kTestTaskId
                                configuration:CreateTestConfiguration()],
            nil);
}

// Tests that requesting a task returns nil when there is no active foreground
// scene.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestRequestTaskFailsWhenNoForegroundActiveScene) {
  app_state_helper_.foregroundActiveScene = nil;

  EXPECT_EQ([agent_ requestTaskWithIdentifier:kTestTaskId
                                configuration:CreateTestConfiguration()],
            nil);
}

// Tests that requesting a task returns nil if system registration fails.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestRequestTaskFailsWhenRegistrationFails) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    StubScheduler(nullptr, /*registration_success=*/NO);

    EXPECT_EQ([agent_ requestTaskWithIdentifier:kTestTaskId
                                  configuration:CreateTestConfiguration()],
              nil);
  }
}

// Tests that requesting a task returns nil if system submission fails.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestRequestTaskFailsWhenSubmissionFails) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    NSError* error = [NSError errorWithDomain:@"org.chromium.test"
                                         code:1
                                     userInfo:nil];
    StubScheduler(nullptr, /*registration_success=*/YES,
                  /*submission_success=*/NO, error);

    EXPECT_EQ([agent_ requestTaskWithIdentifier:kTestTaskId
                                  configuration:CreateTestConfiguration()],
              nil);
  }
}

// Tests that task registration and submission succeed with expected parameters,
// auto-incremented identifiers, and complete successfully.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestRequestTaskRegistersAndSubmitsWithSystem) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    __block void (^captured_launch_handler)(BGTask*) = nil;
    __block BGContinuedProcessingTaskRequest* captured_request = nil;
    OCMStub([mock_scheduler_
                registerForTaskWithIdentifier:[OCMArg any]
                                   usingQueue:dispatch_get_main_queue()
                                launchHandler:[OCMArg checkWithBlock:^BOOL(
                                                          id value) {
                                  captured_launch_handler = [value copy];
                                  return YES;
                                }]])
        .andReturn(YES);
    OCMStub([mock_scheduler_
                submitTaskRequest:[OCMArg checkWithBlock:^BOOL(id value) {
                  captured_request = value;
                  return [value
                      isKindOfClass:[BGContinuedProcessingTaskRequest class]];
                }]
                            error:[OCMArg setTo:nil]])
        .andReturn(YES);

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];

    ASSERT_NE(context, nil);
    ASSERT_NE(captured_request, nil);
    EXPECT_NSEQ(captured_request.title, kTestTaskTitle);
    EXPECT_NSEQ(captured_request.subtitle, kTestTaskSubtitle);
    ASSERT_NE(captured_launch_handler, nil);

    NSString* expected_segment = [NSString
        stringWithFormat:@".continuedProcessingTask.%@.", kTestTaskId];
    EXPECT_TRUE([context.taskIdentifier containsString:expected_segment]);

    // Simulate system delivering the task on the main queue.
    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([mock_task identifier]).andReturn(context.taskIdentifier);
    OCMStub([(BGContinuedProcessingTask*)mock_task progress])
        .andReturn([NSProgress progressWithTotalUnitCount:100]);
    OCMExpect([mock_task setTaskCompletedWithSuccess:YES]);

    captured_launch_handler(mock_task);
    [context setTaskCompletedWithSuccess:YES];

    EXPECT_OCMOCK_VERIFY(mock_task);
  }
}

// Tests that the agent retains active task contexts even if the caller drops
// its reference, and removes it from active tasks upon completion.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestAgentRetainsContextAcrossCallerHandleDrop) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    __weak BackgroundContinuedProcessingTaskContext* weak_context = nil;
    NSString* task_identifier = nil;

    @autoreleasepool {
      BackgroundContinuedProcessingTaskContext* context =
          [agent_ requestTaskWithIdentifier:kTestTaskId
                              configuration:CreateTestConfiguration()];
      ASSERT_NE(context, nil);
      weak_context = context;
      task_identifier = context.taskIdentifier;
      // Caller drops context handle.
      context = nil;
    }

    // Context should still be alive because the agent retains it in
    // `_activeTasks`.
    ASSERT_NE(weak_context, nil);
    ASSERT_NE(captured_launch_handler, nil);

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([mock_task identifier]).andReturn(task_identifier);
    OCMStub([(BGContinuedProcessingTask*)mock_task progress])
        .andReturn([NSProgress progressWithTotalUnitCount:100]);
    OCMExpect([mock_task setTaskCompletedWithSuccess:YES]);

    captured_launch_handler(mock_task);

    // Completing the retained task should notify the mock task and clean up
    // from the agent.
    @autoreleasepool {
      [weak_context setTaskCompletedWithSuccess:YES];
    }
    EXPECT_OCMOCK_VERIFY(mock_task);

    // Now that the task has completed, the agent should have released it.
    EXPECT_EQ(weak_context, nil);
  }
}

// Tests that if the system delivers an unrecognized task identifier,
// the OS task is safely completed with failure.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestLaunchDeliveryWithUnknownIdentifierFailsOSTask) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];
    ASSERT_NE(context, nil);
    ASSERT_NE(captured_launch_handler, nil);

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([mock_task identifier]).andReturn(@"unknown.unregistered.task.id");
    OCMExpect([mock_task setTaskCompletedWithSuccess:NO]);

    captured_launch_handler(mock_task);
    EXPECT_OCMOCK_VERIFY(mock_task);

    [context setTaskCompletedWithSuccess:YES];
  }
}

// Tests that if the system delivers a task that is not a
// `BGContinuedProcessingTask`, the OS task is safely completed with failure.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestLaunchDeliveryWithUnexpectedTaskClassFailsOSTask) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];
    ASSERT_NE(context, nil);
    ASSERT_NE(captured_launch_handler, nil);

    id mock_generic_task = OCMClassMock([BGTask class]);
    OCMExpect([mock_generic_task setTaskCompletedWithSuccess:NO]);

    captured_launch_handler(mock_generic_task);
    EXPECT_OCMOCK_VERIFY(mock_generic_task);

    [context setTaskCompletedWithSuccess:YES];
  }
}

// Tests that completing the context before the OS delivers the underlying
// task cancels the scheduled request in `BGTaskScheduler`, and safely marks
// any subsequent system delivery as completed with failure.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestEarlyCompletionSuccessBeforeOSTaskDelivery) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];
    ASSERT_NE(context, nil);
    ASSERT_NE(captured_launch_handler, nil);

    NSString* task_identifier = context.taskIdentifier;

    OCMExpect(
        [mock_scheduler_ cancelTaskRequestWithIdentifier:task_identifier]);

    [context setTaskCompletedWithSuccess:YES];
    EXPECT_TRUE(context.isCompleted);
    EXPECT_OCMOCK_VERIFY(mock_scheduler_);

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([mock_task identifier]).andReturn(task_identifier);
    OCMExpect([mock_task setTaskCompletedWithSuccess:NO]);

    captured_launch_handler(mock_task);
    EXPECT_OCMOCK_VERIFY(mock_task);
  }
}

// Tests that failing the context before the OS delivers the underlying task
// cancels the scheduled request in `BGTaskScheduler`, and safely marks
// any subsequent system delivery as completed with failure.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestEarlyCompletionFailureBeforeOSTaskDelivery) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];
    ASSERT_NE(context, nil);
    ASSERT_NE(captured_launch_handler, nil);

    NSString* task_identifier = context.taskIdentifier;

    OCMExpect(
        [mock_scheduler_ cancelTaskRequestWithIdentifier:task_identifier]);

    [context setTaskCompletedWithSuccess:NO];
    EXPECT_TRUE(context.isCompleted);
    EXPECT_OCMOCK_VERIFY(mock_scheduler_);

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([mock_task identifier]).andReturn(task_identifier);
    OCMExpect([mock_task setTaskCompletedWithSuccess:NO]);

    captured_launch_handler(mock_task);
    EXPECT_OCMOCK_VERIFY(mock_task);
  }
}

// Tests that deallocating the agent completes all active tasks with failure.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestAgentDeallocCompletesActiveTasks) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    StubScheduler();

    BackgroundContinuedProcessingTaskContext* context = nil;

    @autoreleasepool {
      id local_app_state = OCMClassMock([AppState class]);
      OCMStub([local_app_state foregroundActiveScene])
          .andReturn(mock_scene_state_);

      auto* local_agent = [[BackgroundContinuedProcessingAppAgent alloc] init];
      local_agent.appState = local_app_state;
      context =
          [local_agent requestTaskWithIdentifier:kTestTaskId
                                   configuration:CreateTestConfiguration()];
      ASSERT_NE(context, nil);
      EXPECT_FALSE(context.isCompleted);
      local_agent = nil;
      [local_app_state stopMocking];
    }

    EXPECT_TRUE(context.isCompleted);
  }
}

// Tests that if the agent is deallocated before the OS invokes `launchHandler`,
// the task is safely marked completed with failure.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestLaunchHandlerWhenAgentDeallocatedFailsTask) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    @autoreleasepool {
      id local_app_state = OCMClassMock([AppState class]);
      OCMStub([local_app_state foregroundActiveScene])
          .andReturn(mock_scene_state_);

      auto* local_agent = [[BackgroundContinuedProcessingAppAgent alloc] init];
      local_agent.appState = local_app_state;
      BackgroundContinuedProcessingTaskContext* context =
          [local_agent requestTaskWithIdentifier:kTestTaskId
                                   configuration:CreateTestConfiguration()];
      ASSERT_NE(context, nil);
      ASSERT_NE(captured_launch_handler, nil);
      local_agent = nil;
      [local_app_state stopMocking];
    }

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMExpect([mock_task setTaskCompletedWithSuccess:NO]);

    captured_launch_handler(mock_task);
    EXPECT_OCMOCK_VERIFY(mock_task);
  }
}
