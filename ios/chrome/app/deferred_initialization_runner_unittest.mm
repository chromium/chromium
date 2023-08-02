// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/deferred_initialization_runner.h"

#import "base/test/ios/wait_util.h"
#import "base/test/test_timeouts.h"
#import "base/time/time.h"
#import "testing/platform_test.h"

using DeferredInitializationRunnerTest = PlatformTest;

TEST_F(DeferredInitializationRunnerTest, TestSharedInstance) {
  EXPECT_TRUE([DeferredInitializationRunner sharedInstance]);
  // Cancelling a non-existing block does nothing.
  [[DeferredInitializationRunner sharedInstance]
      cancelBlockNamed:@"Invalid Name"];
}

// Tests that all blocks added on the queue are executed after a delay.
TEST_F(DeferredInitializationRunnerTest, TestRunBlockSequentially) {
  // Setup.
  __block bool firstFlag = NO;
  __block bool secondFlag = NO;
  DeferredInitializationRunner* runner =
      [DeferredInitializationRunner sharedInstance];
  ProceduralBlock firstBlock = ^{
    EXPECT_FALSE(firstFlag);
    firstFlag = YES;
  };
  ProceduralBlock secondBlock = ^{
    EXPECT_FALSE(secondFlag);
    secondFlag = YES;
  };
  ConditionBlock secondBlockRun = ^bool {
    return secondFlag;
  };
  runner.delayBetweenBlocks = 0.01;
  runner.delayBeforeFirstBlock = 0.01;

  [runner enqueueBlockNamed:@"first block" block:firstBlock];
  [runner enqueueBlockNamed:@"second block" block:secondBlock];

  ASSERT_FALSE(firstFlag);
  ASSERT_FALSE(secondFlag);
  EXPECT_EQ(2U, [runner numberOfBlocksRemaining]);

  // Action.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), secondBlockRun));

  // Test.
  EXPECT_TRUE(firstFlag);
  EXPECT_TRUE(secondFlag);
  EXPECT_EQ(0U, [runner numberOfBlocksRemaining]);
}

// Tests that runBlockIfNecessary does not execute the block if it has already
// been executed and runs synchronously the one not executed.
TEST_F(DeferredInitializationRunnerTest, TestRunBlock) {
  // Setup.
  __block bool quickFlag = NO;
  __block bool slowFlag = NO;
  DeferredInitializationRunner* runner =
      [DeferredInitializationRunner sharedInstance];
  ProceduralBlock quickBlock = ^{
    EXPECT_FALSE(quickFlag);
    quickFlag = YES;
    // Make sure we have time to go back to this test before running the second
    // task.
    runner.delayBetweenBlocks = 1;
  };
  ConditionBlock quickBlockRun = ^bool {
    return quickFlag;
  };
  ProceduralBlock slowBlock = ^{
    EXPECT_FALSE(slowFlag);
    slowFlag = YES;
  };
  runner.delayBeforeFirstBlock = 0.01;

  // Action.
  [runner enqueueBlockNamed:@"quick block" block:quickBlock];
  [runner enqueueBlockNamed:@"slow block" block:slowBlock];

  // Test.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), quickBlockRun));
  EXPECT_TRUE(quickFlag);
  EXPECT_FALSE(slowFlag);
  EXPECT_EQ(1U, [runner numberOfBlocksRemaining]);
  [runner runBlockIfNecessary:@"quick block"];
  [runner runBlockIfNecessary:@"slow block"];
  EXPECT_TRUE(quickFlag);
  EXPECT_TRUE(slowFlag);
  EXPECT_EQ(0U, [runner numberOfBlocksRemaining]);
}

// Tests that a block is not executed when cancelled and it is removed from the
// remaining blocks list.
TEST_F(DeferredInitializationRunnerTest, TestCancelBlock) {
  // Setup.
  __block BOOL blockFinished = NO;
  DeferredInitializationRunner* runner =
      [DeferredInitializationRunner sharedInstance];
  runner.delayBeforeFirstBlock = 0.01;
  runner.delayBetweenBlocks = 0.01;

  [runner enqueueBlockNamed:@"cancel me"
                      block:^{
                        blockFinished = YES;
                      }];
  ASSERT_EQ(1U, [runner numberOfBlocksRemaining]);

  // Action.
  [runner cancelBlockNamed:@"cancel me"];

  // Test.
  EXPECT_FALSE(blockFinished);
  EXPECT_EQ(0U, [runner numberOfBlocksRemaining]);
}

// Tests that a cancelled block will do nothing when run by name.
TEST_F(DeferredInitializationRunnerTest, TestCancelledBlockDoNothing) {
  // Setup.
  __block BOOL blockFinished = NO;
  DeferredInitializationRunner* runner =
      [DeferredInitializationRunner sharedInstance];
  runner.delayBeforeFirstBlock = 0.01;
  runner.delayBetweenBlocks = 0.01;

  [runner enqueueBlockNamed:@"cancel me"
                      block:^{
                        blockFinished = YES;
                      }];

  // Action.
  [runner cancelBlockNamed:@"cancel me"];
  [runner runBlockIfNecessary:@"cancel me"];

  // Test: expect false, the block should never be executed because it was
  // cancelled before it started running.
  EXPECT_FALSE(blockFinished);
}

// Tests that adding a block with the same name as an existing block will
// override the existing one.
TEST_F(DeferredInitializationRunnerTest, TestSecondBlockInvalidatesFirst) {
  // Setup.
  __block int blockRunCount = 0;
  ProceduralBlock runBlock = ^{
    ++blockRunCount;
  };
  DeferredInitializationRunner* runner =
      [DeferredInitializationRunner sharedInstance];
  runner.delayBeforeFirstBlock = 0.01;
  runner.delayBetweenBlocks = 0.01;

  // Action.
  [runner enqueueBlockNamed:@"multiple" block:runBlock];
  [runner enqueueBlockNamed:@"multiple" block:runBlock];

  // Test: `runBlock` was executed only once.
  EXPECT_EQ(1U, [runner numberOfBlocksRemaining]);
  [runner runBlockIfNecessary:@"multiple"];
  EXPECT_EQ(0U, [runner numberOfBlocksRemaining]);
  EXPECT_EQ(1, blockRunCount);
}
