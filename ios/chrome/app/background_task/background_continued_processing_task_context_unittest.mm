// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"

#import <BackgroundTasks/BackgroundTasks.h>

#import "base/test/gtest_util.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

NSString* const kTestTaskTitle = @"Foo";
NSString* const kTestTaskSubtitle = @"Bar";

}  // namespace

#pragma mark - BackgroundContinuedProcessingTaskContextTest

class BackgroundContinuedProcessingTaskContextTest : public PlatformTest {
 public:
  BackgroundContinuedProcessingTaskContextTest() {
    mock_scheduler_ = OCMClassMock([BGTaskScheduler class]);
    OCMStub([mock_scheduler_ sharedScheduler]).andReturn(mock_scheduler_);
  }

  ~BackgroundContinuedProcessingTaskContextTest() override {
    [mock_scheduler_ stopMocking];
  }

  BackgroundContinuedProcessingTaskContext* CreateTestContext(
      NSString* task_id = @"test.id",
      ProceduralBlock expiration_handler =
          ^{
          },
      ProceduralBlock finish_handler = nil) {
    BackgroundContinuedProcessingTaskConfiguration* config =
        [[BackgroundContinuedProcessingTaskConfiguration alloc]
                initWithTitle:kTestTaskTitle
            expirationHandler:expiration_handler];
    config.subtitle = kTestTaskSubtitle;
    return [[BackgroundContinuedProcessingTaskContext alloc]
        initWithTaskIdentifier:task_id
                 configuration:config
                 finishHandler:finish_handler];
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  id mock_scheduler_;
};

// Tests that completing the context before the OS delivers the underlying task
// cancels the pending task request, and immediately marks the system task with
// the recorded outcome (success or failure) upon delivery.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestEarlyCompletionBeforeOSTaskDelivery) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    // Early completion with success.
    {
      OCMExpect([mock_scheduler_
          cancelTaskRequestWithIdentifier:@"early.success.test.id"]);

      BackgroundContinuedProcessingTaskContext* context =
          CreateTestContext(@"early.success.test.id");
      [context setTaskCompletedWithSuccess:YES];
      EXPECT_TRUE(context.isCompleted);
      EXPECT_OCMOCK_VERIFY(mock_scheduler_);

      id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
      OCMExpect([mock_task setTaskCompletedWithSuccess:YES]);

      [context attachUnderlyingTask:mock_task];
      EXPECT_OCMOCK_VERIFY(mock_task);
    }

    // Early completion with failure.
    {
      OCMExpect([mock_scheduler_
          cancelTaskRequestWithIdentifier:@"early.failure.test.id"]);

      BackgroundContinuedProcessingTaskContext* context =
          CreateTestContext(@"early.failure.test.id");
      [context setTaskCompletedWithSuccess:NO];
      EXPECT_TRUE(context.isCompleted);
      EXPECT_OCMOCK_VERIFY(mock_scheduler_);

      id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
      OCMExpect([mock_task setTaskCompletedWithSuccess:NO]);

      [context attachUnderlyingTask:mock_task];
      EXPECT_OCMOCK_VERIFY(mock_task);
    }
  }
}

// Tests that multiple completion calls are idempotent and mutations after
// completion are ignored.
TEST_F(BackgroundContinuedProcessingTaskContextTest, TestPostCompletionSafety) {
  BackgroundContinuedProcessingTaskContext* context =
      CreateTestContext(@"mutation.test.id");

  [context setTaskCompletedWithSuccess:YES];
  EXPECT_TRUE(context.isCompleted);

  // Subsequent completions should be safe no-ops.
  [context setTaskCompletedWithSuccess:YES];
  [context setTaskCompletedWithSuccess:NO];
  EXPECT_TRUE(context.isCompleted);

  // Mutations after completion must be ignored.
  context.title = @"New Title";
  context.subtitle = @"New Subtitle";
  [context updateTitle:@"Another Title" subtitle:@"Another Subtitle"];
  EXPECT_NSEQ(context.title, kTestTaskTitle);
  EXPECT_NSEQ(context.subtitle, kTestTaskSubtitle);

  [context incrementProgressByUnits:50];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.0);

  [context setCompletedUnits:50];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.0);
}

// Tests that setting an empty title fails.
TEST_F(BackgroundContinuedProcessingTaskContextTest, TestEmptyTitleFails) {
  BackgroundContinuedProcessingTaskContext* context =
      CreateTestContext(@"empty.title.test.id");

  EXPECT_DEATH_IF_SUPPORTED(context.title = @"", "");
  EXPECT_DEATH_IF_SUPPORTED([context updateTitle:@"" subtitle:@"Bar"], "");
}

// Tests that progress updates update `fractionCompleted` correctly and clamp
// to valid bounds [0, totalUnits].
TEST_F(BackgroundContinuedProcessingTaskContextTest, TestProgressTracking) {
  BackgroundContinuedProcessingTaskContext* context =
      CreateTestContext(@"progress.test.id");

  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.0);

  [context incrementProgressByUnits:25];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.25);

  [context setCompletedUnits:50];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.5);

  // Clamping to `totalUnitCount` (default 100).
  [context incrementProgressByUnits:60];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 1.0);

  // Clamping to 0.
  [context setCompletedUnits:-10];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.0);
}

// Tests that updating progress syncs to the underlying OS task when attached.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestProgressSyncsToUnderlyingTask) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    BackgroundContinuedProcessingTaskContext* context =
        CreateTestContext(@"progress.sync.test.id");
    [context setCompletedUnits:30];

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    NSProgress* task_progress = [NSProgress progressWithTotalUnitCount:100];
    OCMStub([(BGContinuedProcessingTask*)mock_task progress])
        .andReturn(task_progress);

    [context attachUnderlyingTask:mock_task];
    EXPECT_EQ(task_progress.completedUnitCount, 30);

    [context incrementProgressByUnits:20];
    EXPECT_EQ(task_progress.completedUnitCount, 50);

    [context setCompletedUnits:80];
    EXPECT_EQ(task_progress.completedUnitCount, 80);
  }
}

// Tests that system expiration executes the client handler before notifying the
// OS of task completion, invokes the finish handler, and marks the context
// completed.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestTaskExpirationSynchronous) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    __block BOOL expiration_called = NO;
    __block BOOL os_completed_called = NO;
    __block BOOL expiration_called_before_os_completion = NO;
    __block BOOL finish_handler_called = NO;

    BackgroundContinuedProcessingTaskContext* context = CreateTestContext(
        @"expiration.test.id",
        ^{
          expiration_called = YES;
          if (!os_completed_called) {
            expiration_called_before_os_completion = YES;
          }
        },
        ^{
          finish_handler_called = YES;
        });

    __block void (^captured_expiration_handler)(void) = nil;
    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([(BGContinuedProcessingTask*)mock_task progress])
        .andReturn([NSProgress progressWithTotalUnitCount:100]);
    OCMStub(
        [mock_task setExpirationHandler:[OCMArg checkWithBlock:^BOOL(id value) {
                     captured_expiration_handler = [value copy];
                     return YES;
                   }]]);
    OCMStub([mock_task setTaskCompletedWithSuccess:NO])
        .andDo(^(NSInvocation* invocation) {
          os_completed_called = YES;
        });

    [context attachUnderlyingTask:mock_task];
    ASSERT_NE(captured_expiration_handler, nil);

    captured_expiration_handler();

    EXPECT_TRUE(expiration_called);
    EXPECT_TRUE(os_completed_called);
    EXPECT_TRUE(expiration_called_before_os_completion);
    EXPECT_TRUE(finish_handler_called);
    EXPECT_TRUE(context.isCompleted);
  }
}

// Tests that if the system invokes the expiration handler on a background
// thread, execution safely hops to the main thread and completes the task.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestTaskExpirationOffThread) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    __block BOOL expiration_called = NO;
    __block BOOL finish_handler_called = NO;
    __block BOOL os_completed_called = NO;
    base::test::TestFuture<void> future;
    base::RepeatingClosure done_callback = future.GetRepeatingCallback();

    BackgroundContinuedProcessingTaskContext* context = CreateTestContext(
        @"offthread.expiration.test.id",
        ^{
          EXPECT_TRUE(NSThread.isMainThread);
          expiration_called = YES;
        },
        ^{
          EXPECT_TRUE(NSThread.isMainThread);
          finish_handler_called = YES;
          done_callback.Run();
        });

    __block void (^captured_expiration_handler)(void) = nil;
    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([(BGContinuedProcessingTask*)mock_task progress])
        .andReturn([NSProgress progressWithTotalUnitCount:100]);
    OCMStub(
        [mock_task setExpirationHandler:[OCMArg checkWithBlock:^BOOL(id value) {
                     captured_expiration_handler = [value copy];
                     return YES;
                   }]]);
    OCMStub([mock_task setTaskCompletedWithSuccess:NO])
        .andDo(^(NSInvocation* invocation) {
          os_completed_called = YES;
        });

    [context attachUnderlyingTask:mock_task];
    ASSERT_NE(captured_expiration_handler, nil);

    // Invoke the expiration handler on a background thread.
    dispatch_async(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
          captured_expiration_handler();
        });

    EXPECT_TRUE(future.Wait());

    EXPECT_TRUE(expiration_called);
    EXPECT_TRUE(finish_handler_called);
    EXPECT_TRUE(os_completed_called);
    EXPECT_TRUE(context.isCompleted);
  }
}

// Tests that dropping context while a task is actively attached marks the
// underlying OS task completed with failure.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestContextDeallocationFailsActiveOSTask) {
  if (!@available(iOS 26.0, *)) {
    GTEST_SKIP() << "BGContinuedProcessingTask requires iOS 26.0+.";
  }

  if (@available(iOS 26.0, *)) {
    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([(BGContinuedProcessingTask*)mock_task progress])
        .andReturn([NSProgress progressWithTotalUnitCount:100]);
    OCMExpect([mock_task setTaskCompletedWithSuccess:NO]);

    @autoreleasepool {
      BackgroundContinuedProcessingTaskContext* context =
          CreateTestContext(@"drop.active.test.id");
      [context attachUnderlyingTask:mock_task];
      context = nil;
    }

    EXPECT_OCMOCK_VERIFY(mock_task);
  }
}
