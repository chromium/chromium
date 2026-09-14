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
      NSString* taskId = @"test.id",
      ProceduralBlock expirationHandler =
          ^{
          },
      ProceduralBlock finishHandler = nil) {
    BackgroundContinuedProcessingTaskConfiguration* config =
        [[BackgroundContinuedProcessingTaskConfiguration alloc]
                initWithTitle:kTestTaskTitle
            expirationHandler:expirationHandler];
    config.subtitle = kTestTaskSubtitle;
    return [[BackgroundContinuedProcessingTaskContext alloc]
        initWithTaskIdentifier:taskId
                 configuration:config
                 finishHandler:finishHandler];
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

      id mockTask = OCMClassMock([BGContinuedProcessingTask class]);
      OCMExpect([mockTask setTaskCompletedWithSuccess:YES]);

      [context attachUnderlyingTask:mockTask];
      EXPECT_OCMOCK_VERIFY(mockTask);
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

      id mockTask = OCMClassMock([BGContinuedProcessingTask class]);
      OCMExpect([mockTask setTaskCompletedWithSuccess:NO]);

      [context attachUnderlyingTask:mockTask];
      EXPECT_OCMOCK_VERIFY(mockTask);
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
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 1.0);

  [context setCompletedUnits:50];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 1.0);

  [context incrementStepProgress];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 1.0);
  EXPECT_EQ(context.completedUnits, kDefaultTotalUnitsOfProgress);

  BackgroundContinuedProcessingTaskContext* failureContext =
      CreateTestContext(@"mutation.failure.test.id");
  [failureContext setTaskCompletedWithSuccess:NO];
  EXPECT_TRUE(failureContext.isCompleted);
  [failureContext incrementProgressByUnits:50];
  EXPECT_DOUBLE_EQ(failureContext.fractionCompleted, 0.0);
  [failureContext setCompletedUnits:50];
  EXPECT_DOUBLE_EQ(failureContext.fractionCompleted, 0.0);
  [failureContext incrementStepProgress];
  EXPECT_DOUBLE_EQ(failureContext.fractionCompleted, 0.0);
  EXPECT_EQ(failureContext.completedUnits, 0);
}

// Tests that setting an empty title fails.
TEST_F(BackgroundContinuedProcessingTaskContextTest, TestEmptyTitleFails) {
  BackgroundContinuedProcessingTaskContext* context =
      CreateTestContext(@"empty.title.test.id");

  EXPECT_DEATH_IF_SUPPORTED(context.title = @"", "");
  EXPECT_DEATH_IF_SUPPORTED([context updateTitle:@"" subtitle:@"Bar"], "");
}

// Tests that configuring invalid progress invariants triggers a crash.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestInvalidConfigurationFails) {
  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:kTestTaskTitle
          expirationHandler:^{
          }];

  EXPECT_DEATH_IF_SUPPORTED(config.totalUnits = 0, "");
  EXPECT_DEATH_IF_SUPPORTED(config.totalUnits = -1, "");
  EXPECT_DEATH_IF_SUPPORTED(config.expectedStepCount = 0, "");
  EXPECT_DEATH_IF_SUPPORTED(config.expectedStepCount = -1, "");
}

// Tests that `incrementStepProgress` behaves correctly for small `totalUnits`
// edge cases without crashing, stalling, or exceeding `totalUnits` - 1.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestIncrementStepProgressEdgeCases) {
  // Edge case: `totalUnits` = 1.
  {
    BackgroundContinuedProcessingTaskConfiguration* config =
        [[BackgroundContinuedProcessingTaskConfiguration alloc]
                initWithTitle:kTestTaskTitle
            expirationHandler:^{
            }];
    config.totalUnits = 1;
    BackgroundContinuedProcessingTaskContext* context =
        [[BackgroundContinuedProcessingTaskContext alloc]
            initWithTaskIdentifier:@"edge.1.test.id"
                     configuration:config
                     finishHandler:nil];

    // For `totalUnits` = 1, ceiling is 0; completed units must remain 0 before
    // completion.
    [context incrementStepProgress];
    EXPECT_EQ(context.completedUnits, 0);
    EXPECT_LT(context.completedUnits, context.totalUnits);

    [context setTaskCompletedWithSuccess:YES];
    EXPECT_EQ(context.completedUnits, 1);
  }

  // Edge case: `totalUnits` = 2.
  {
    BackgroundContinuedProcessingTaskConfiguration* config =
        [[BackgroundContinuedProcessingTaskConfiguration alloc]
                initWithTitle:kTestTaskTitle
            expirationHandler:^{
            }];
    config.totalUnits = 2;
    BackgroundContinuedProcessingTaskContext* context =
        [[BackgroundContinuedProcessingTaskContext alloc]
            initWithTaskIdentifier:@"edge.2.test.id"
                     configuration:config
                     finishHandler:nil];

    // For `totalUnits` = 2, linear and asymptotic ceilings are 1.
    [context incrementStepProgress];
    EXPECT_EQ(context.completedUnits, 1);

    // Stepping past ceiling remains clamped at 1.
    [context incrementStepProgress];
    EXPECT_EQ(context.completedUnits, 1);
    EXPECT_LT(context.completedUnits, context.totalUnits);

    [context setTaskCompletedWithSuccess:YES];
    EXPECT_EQ(context.completedUnits, 2);
  }

  // Edge case: Low-resolution configuration where `linearCeiling` <
  // `expectedStepCount` (`stepRatio` < 1.0). Each step must strictly advance by
  // at least +1 unit until reaching the linear and asymptotic ceilings.
  {
    BackgroundContinuedProcessingTaskConfiguration* config =
        [[BackgroundContinuedProcessingTaskConfiguration alloc]
                initWithTitle:kTestTaskTitle
            expirationHandler:^{
            }];
    config.totalUnits = 10;
    config.expectedStepCount = 18;
    BackgroundContinuedProcessingTaskContext* context =
        [[BackgroundContinuedProcessingTaskContext alloc]
            initWithTaskIdentifier:@"edge.lowres.test.id"
                     configuration:config
                     finishHandler:nil];

    // Linear ceiling is 7. Verify each step strictly increments by at least 1
    // unit.
    for (int64_t expectedUnits = 1; expectedUnits <= 7; ++expectedUnits) {
      [context incrementStepProgress];
      EXPECT_EQ(context.completedUnits, expectedUnits);
    }

    // Step into asymptotic phase: ceiling is 9 (std::min(9, round(10 * 0.98))).
    [context incrementStepProgress];
    EXPECT_EQ(context.completedUnits, 8);

    [context incrementStepProgress];
    EXPECT_EQ(context.completedUnits, 9);

    // Capped at asymptotic ceiling (9). Must not reach `totalUnits` (10).
    [context incrementStepProgress];
    EXPECT_EQ(context.completedUnits, 9);
    EXPECT_LT(context.completedUnits, context.totalUnits);

    [context setTaskCompletedWithSuccess:YES];
    EXPECT_EQ(context.completedUnits, 10);
  }
}

// Tests that progress updates update `fractionCompleted` correctly and clamp
// to valid bounds [0, totalUnits].
TEST_F(BackgroundContinuedProcessingTaskContextTest, TestProgressTracking) {
  BackgroundContinuedProcessingTaskContext* context =
      CreateTestContext(@"progress.test.id");

  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.0);
  EXPECT_EQ(context.completedUnits, 0);
  EXPECT_EQ(context.totalUnits, kDefaultTotalUnitsOfProgress);

  [context incrementProgressByUnits:0];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.0);
  EXPECT_EQ(context.completedUnits, 0);
  EXPECT_DEATH_IF_SUPPORTED([context incrementProgressByUnits:-5], "");

  [context incrementProgressByUnits:250];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.25);
  EXPECT_EQ(context.completedUnits, 250);

  [context setCompletedUnits:500];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.5);
  EXPECT_EQ(context.completedUnits, 500);

  // Clamping to `totalUnitCount` (default 1000).
  [context incrementProgressByUnits:600];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 1.0);
  EXPECT_EQ(context.completedUnits, 1000);

  // Clamping to 0.
  [context setCompletedUnits:-10];
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.0);
  EXPECT_EQ(context.completedUnits, 0);
}

// Tests that `incrementStepProgress` advances linearly up to the linear
// threshold and asymptotically thereafter without exceeding the asymptotic
// ceiling.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestIncrementStepProgressLinearAndAsymptotic) {
  BackgroundContinuedProcessingTaskContext* context =
      CreateTestContext(@"step.progress.test.id");

  const int64_t linearCeiling = 700;
  const int64_t asymptoticCeiling = 980;

  EXPECT_EQ(context.completedUnits, 0);

  // Advance to halfway of linear phase.
  for (int i = 0; i < kDefaultExpectedStepCount / 2; ++i) {
    [context incrementStepProgress];
  }
  EXPECT_EQ(context.completedUnits, 350);

  // Advance to end of linear phase.
  for (int i = kDefaultExpectedStepCount / 2; i < kDefaultExpectedStepCount;
       ++i) {
    [context incrementStepProgress];
  }
  EXPECT_EQ(context.completedUnits, linearCeiling);

  // First step in asymptotic phase: remaining = 980 - 700 = 280. 280 / 25 = 11.
  // Expected = 700 + 11 = 711.
  [context incrementStepProgress];
  EXPECT_EQ(context.completedUnits, 711);

  // Subsequent steps should monotonically increase toward the asymptotic
  // ceiling without regressing or stalling.
  int64_t previousUnits = context.completedUnits;
  for (int i = 0; i < 200; ++i) {
    [context incrementStepProgress];
    EXPECT_GE(context.completedUnits, previousUnits);
    if (previousUnits < asymptoticCeiling) {
      EXPECT_GT(context.completedUnits, previousUnits);
    } else {
      EXPECT_EQ(context.completedUnits, asymptoticCeiling);
    }
    previousUnits = context.completedUnits;
  }
  EXPECT_EQ(context.completedUnits, asymptoticCeiling);
  EXPECT_LT(context.fractionCompleted, 1.0);
}

// Tests that a custom `expectedStepCount` value alters the linear progression
// rate.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestIncrementStepProgressCustomExpectedStepCount) {
  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:kTestTaskTitle
          expirationHandler:^{
          }];
  config.expectedStepCount = 10;
  BackgroundContinuedProcessingTaskContext* context =
      [[BackgroundContinuedProcessingTaskContext alloc]
          initWithTaskIdentifier:@"custom.expected.steps.test.id"
                   configuration:config
                   finishHandler:nil];

  const int64_t linearCeiling = 700;
  const int64_t asymptoticCeiling = 980;

  for (int i = 0; i < 10; ++i) {
    [context incrementStepProgress];
  }
  EXPECT_EQ(context.completedUnits, linearCeiling);

  // 11th step is in asymptotic phase.
  [context incrementStepProgress];
  EXPECT_GT(context.completedUnits, linearCeiling);
  EXPECT_LE(context.completedUnits, asymptoticCeiling);

  // Edge case: `expectedStepCount` = 1 advances immediately to `linearCeiling`.
  {
    BackgroundContinuedProcessingTaskConfiguration* singleStepConfig =
        [[BackgroundContinuedProcessingTaskConfiguration alloc]
                initWithTitle:kTestTaskTitle
            expirationHandler:^{
            }];
    singleStepConfig.expectedStepCount = 1;
    BackgroundContinuedProcessingTaskContext* singleStepContext =
        [[BackgroundContinuedProcessingTaskContext alloc]
            initWithTaskIdentifier:@"single.step.expected.test.id"
                     configuration:singleStepConfig
                     finishHandler:nil];

    [singleStepContext incrementStepProgress];
    EXPECT_EQ(singleStepContext.completedUnits, linearCeiling);

    // Subsequent step enters asymptotic phase.
    [singleStepContext incrementStepProgress];
    EXPECT_GT(singleStepContext.completedUnits, linearCeiling);
    EXPECT_LE(singleStepContext.completedUnits, asymptoticCeiling);
  }
}

// Tests that a custom `totalUnits` value scales both the linear and asymptotic
// progression phases proportionally.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestIncrementStepProgressCustomTotalUnits) {
  constexpr int64_t kCustomTotalUnits = 2000;
  const int64_t linearCeiling = 1400;
  const int64_t asymptoticCeiling = 1960;

  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:kTestTaskTitle
          expirationHandler:^{
          }];
  config.totalUnits = kCustomTotalUnits;
  BackgroundContinuedProcessingTaskContext* context =
      [[BackgroundContinuedProcessingTaskContext alloc]
          initWithTaskIdentifier:@"custom.total.units.test.id"
                   configuration:config
                   finishHandler:nil];

  EXPECT_EQ(context.totalUnits, kCustomTotalUnits);

  // Advance through linear phase.
  for (int i = 0; i < kDefaultExpectedStepCount; ++i) {
    [context incrementStepProgress];
  }
  EXPECT_EQ(context.completedUnits, linearCeiling);
  EXPECT_NEAR(context.fractionCompleted, 0.70, 0.001);

  // First step into asymptotic phase: remaining = 1960 - 1400 = 560.
  // 560 / 25 = 22. Expected units = 1400 + 22 = 1422.
  [context incrementStepProgress];
  EXPECT_EQ(context.completedUnits, 1422);

  // Advance until asymptotic ceiling is reached.
  for (int i = 0; i < 200; ++i) {
    [context incrementStepProgress];
  }
  EXPECT_EQ(context.completedUnits, asymptoticCeiling);
  EXPECT_NEAR(context.fractionCompleted, 0.98, 0.001);
}

// Tests that interleaved manual progress updates and stepped progress updates
// advance smoothly and monotonically without crashing or regressing.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestInterleavedManualAndSteppedProgress) {
  BackgroundContinuedProcessingTaskContext* context =
      CreateTestContext(@"interleaved.progress.test.id");

  // Step once linearly.
  [context incrementStepProgress];
  EXPECT_GT(context.completedUnits, 0);
  const int64_t step1Units = context.completedUnits;

  // Manually increment past current progress.
  [context incrementProgressByUnits:200];
  EXPECT_EQ(context.completedUnits, step1Units + 200);
  const int64_t manual1Units = context.completedUnits;

  // Continue stepping: should resume from inferred step without crashing.
  [context incrementStepProgress];
  EXPECT_GT(context.completedUnits, manual1Units);

  // Manual backward adjustment: decrementing progress should allow stepped
  // progress to seamlessly infer the lower step and advance forward from there.
  [context setCompletedUnits:200];
  EXPECT_EQ(context.completedUnits, 200);
  [context incrementStepProgress];
  EXPECT_GT(context.completedUnits, 200);
  EXPECT_LT(context.completedUnits, 700);

  // Manually jump past linear ceiling (700).
  [context setCompletedUnits:750];
  EXPECT_EQ(context.completedUnits, 750);

  // Stepped progress should now advance in asymptotic phase without crashing.
  [context incrementStepProgress];
  EXPECT_GT(context.completedUnits, 750);
  EXPECT_LE(context.completedUnits, 980);

  // Manually jump past asymptotic ceiling (980) into upper tail [980, 1000).
  [context setCompletedUnits:990];
  EXPECT_EQ(context.completedUnits, 990);

  // Stepping above the asymptotic ceiling must not regress progress back to
  // 980.
  [context incrementStepProgress];
  EXPECT_EQ(context.completedUnits, 990);
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.99);

  // Jump to 100% completion units.
  [context setCompletedUnits:1000];
  EXPECT_EQ(context.completedUnits, 1000);

  // Stepping at maximum units should stay at 1000 without crashing.
  [context incrementStepProgress];
  EXPECT_EQ(context.completedUnits, 1000);
}

// Tests that completing the task with success fills progress to 100%,
// while completing with failure preserves partial progress.
TEST_F(BackgroundContinuedProcessingTaskContextTest,
       TestTaskCompletedWithSuccessFillsProgress) {
  BackgroundContinuedProcessingTaskContext* context =
      CreateTestContext(@"success.progress.test.id");

  [context incrementStepProgress];
  EXPECT_LT(context.completedUnits, kDefaultTotalUnitsOfProgress);

  [context setTaskCompletedWithSuccess:YES];
  EXPECT_EQ(context.completedUnits, kDefaultTotalUnitsOfProgress);
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 1.0);

  BackgroundContinuedProcessingTaskContext* failureContext =
      CreateTestContext(@"failure.progress.test.id");
  [failureContext incrementStepProgress];
  const int64_t unitsBeforeFailure = failureContext.completedUnits;
  [failureContext setTaskCompletedWithSuccess:NO];
  EXPECT_EQ(failureContext.completedUnits, unitsBeforeFailure);
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
    [context setCompletedUnits:300];

    id mockTask = OCMClassMock([BGContinuedProcessingTask class]);
    NSProgress* taskProgress =
        [NSProgress progressWithTotalUnitCount:kDefaultTotalUnitsOfProgress];
    OCMStub([(BGContinuedProcessingTask*)mockTask progress])
        .andReturn(taskProgress);

    [context attachUnderlyingTask:mockTask];
    EXPECT_EQ(taskProgress.completedUnitCount, 300);

    [context incrementProgressByUnits:200];
    EXPECT_EQ(taskProgress.completedUnitCount, 500);

    [context setCompletedUnits:800];
    EXPECT_EQ(taskProgress.completedUnitCount, 800);

    [context incrementStepProgress];
    EXPECT_EQ(taskProgress.completedUnitCount, context.completedUnits);
    EXPECT_GT(taskProgress.completedUnitCount, 800);
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
    __block BOOL expirationCalled = NO;
    __block BOOL osCompletedCalled = NO;
    __block BOOL expirationCalledBeforeOsCompletion = NO;
    __block BOOL finishHandlerCalled = NO;

    BackgroundContinuedProcessingTaskContext* context = CreateTestContext(
        @"expiration.test.id",
        ^{
          expirationCalled = YES;
          if (!osCompletedCalled) {
            expirationCalledBeforeOsCompletion = YES;
          }
        },
        ^{
          finishHandlerCalled = YES;
        });

    __block void (^capturedExpirationHandler)(void) = nil;
    id mockTask = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([(BGContinuedProcessingTask*)mockTask progress])
        .andReturn([NSProgress
            progressWithTotalUnitCount:kDefaultTotalUnitsOfProgress]);
    OCMStub(
        [mockTask setExpirationHandler:[OCMArg checkWithBlock:^BOOL(id value) {
                    capturedExpirationHandler = [value copy];
                    return YES;
                  }]]);
    OCMStub([mockTask setTaskCompletedWithSuccess:NO])
        .andDo(^(NSInvocation* invocation) {
          osCompletedCalled = YES;
        });

    [context attachUnderlyingTask:mockTask];
    ASSERT_NE(capturedExpirationHandler, nil);

    capturedExpirationHandler();

    EXPECT_TRUE(expirationCalled);
    EXPECT_TRUE(osCompletedCalled);
    EXPECT_TRUE(expirationCalledBeforeOsCompletion);
    EXPECT_TRUE(finishHandlerCalled);
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
    __block BOOL expirationCalled = NO;
    __block BOOL finishHandlerCalled = NO;
    __block BOOL osCompletedCalled = NO;
    base::test::TestFuture<void> future;
    base::RepeatingClosure doneCallback = future.GetRepeatingCallback();

    BackgroundContinuedProcessingTaskContext* context = CreateTestContext(
        @"offthread.expiration.test.id",
        ^{
          EXPECT_TRUE(NSThread.isMainThread);
          expirationCalled = YES;
        },
        ^{
          EXPECT_TRUE(NSThread.isMainThread);
          finishHandlerCalled = YES;
          doneCallback.Run();
        });

    __block void (^capturedExpirationHandler)(void) = nil;
    id mockTask = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([(BGContinuedProcessingTask*)mockTask progress])
        .andReturn([NSProgress
            progressWithTotalUnitCount:kDefaultTotalUnitsOfProgress]);
    OCMStub(
        [mockTask setExpirationHandler:[OCMArg checkWithBlock:^BOOL(id value) {
                    capturedExpirationHandler = [value copy];
                    return YES;
                  }]]);
    OCMStub([mockTask setTaskCompletedWithSuccess:NO])
        .andDo(^(NSInvocation* invocation) {
          osCompletedCalled = YES;
        });

    [context attachUnderlyingTask:mockTask];
    ASSERT_NE(capturedExpirationHandler, nil);

    // Invoke the expiration handler on a background thread.
    dispatch_async(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
          capturedExpirationHandler();
        });

    EXPECT_TRUE(future.Wait());

    EXPECT_TRUE(expirationCalled);
    EXPECT_TRUE(finishHandlerCalled);
    EXPECT_TRUE(osCompletedCalled);
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
    id mockTask = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([(BGContinuedProcessingTask*)mockTask progress])
        .andReturn([NSProgress progressWithTotalUnitCount:100]);
    OCMExpect([mockTask setTaskCompletedWithSuccess:NO]);

    @autoreleasepool {
      BackgroundContinuedProcessingTaskContext* context =
          CreateTestContext(@"drop.active.test.id");
      [context attachUnderlyingTask:mockTask];
      context = nil;
    }

    EXPECT_OCMOCK_VERIFY(mockTask);
  }
}
