// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/deferred_initialization_runner.h"

#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "testing/platform_test.h"

class DeferredInitializationRunnerTest : public PlatformTest {
 public:
  DeferredInitializationRunnerTest() {
    _runner = [[DeferredInitializationRunner alloc]
        initWithDelayBetweenBlocks:base::Milliseconds(10)
             delayBeforeFirstBlock:base::Milliseconds(10)];
  }

  DeferredInitializationRunner* runner() { return _runner; }

 private:
  base::test::TaskEnvironment _task_environment;
  DeferredInitializationRunner* _runner;
};

// Tests that cancelling a non-existing block does nothing.
TEST_F(DeferredInitializationRunnerTest, TestSharedInstance) {
  [runner() cancelBlockNamed:@"Invalid Name"];
}

// Tests that all blocks added on the queue are executed after a delay.
TEST_F(DeferredInitializationRunnerTest, TestRunBlockSequentially) {
  base::RunLoop run_loop0;
  [runner() enqueueBlockNamed:@"block0"
                        block:base::CallbackToBlock(run_loop0.QuitClosure())];

  base::RunLoop run_loop1;
  [runner() enqueueBlockNamed:@"block1"
                        block:base::CallbackToBlock(run_loop1.QuitClosure())];

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(2U, [runner() numberOfBlocksRemaining]);

  // Wait for the blocks to be executed.
  run_loop0.Run();
  run_loop1.Run();

  EXPECT_TRUE(run_loop0.AnyQuitCalled());
  EXPECT_TRUE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(0U, [runner() numberOfBlocksRemaining]);
}

// Tests that runBlockIfNecessary does not execute the block if it has already
// been executed and runs synchronously the one not executed.
TEST_F(DeferredInitializationRunnerTest, TestRunBlock) {
  base::RunLoop run_loop0;
  [runner() enqueueBlockNamed:@"block0"
                        block:base::CallbackToBlock(run_loop0.QuitClosure())];

  base::RunLoop run_loop1;
  [runner() enqueueBlockNamed:@"block1"
                        block:base::CallbackToBlock(run_loop1.QuitClosure())];

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(2U, [runner() numberOfBlocksRemaining]);

  // Wait for the first block to be executed.
  run_loop0.Run();

  EXPECT_TRUE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(1U, [runner() numberOfBlocksRemaining]);

  // Manually invoke the blocks.
  [runner() runBlockIfNecessary:@"block0"];
  [runner() runBlockIfNecessary:@"block1"];

  EXPECT_TRUE(run_loop0.AnyQuitCalled());
  EXPECT_TRUE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(0U, [runner() numberOfBlocksRemaining]);
}

// Tests that a block is not executed when cancelled and it is removed from the
// remaining blocks list.
TEST_F(DeferredInitializationRunnerTest, TestCancelBlock) {
  base::RunLoop run_loop;
  [runner() enqueueBlockNamed:@"block"
                        block:base::CallbackToBlock(run_loop.QuitClosure())];

  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(1U, [runner() numberOfBlocksRemaining]);

  // Cancel the block before it is executed.
  [runner() cancelBlockNamed:@"block"];

  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(0U, [runner() numberOfBlocksRemaining]);
}

// Tests that a cancelled block will do nothing when run by name.
TEST_F(DeferredInitializationRunnerTest, TestCancelledBlockDoNothing) {
  base::RunLoop run_loop;
  [runner() enqueueBlockNamed:@"block"
                        block:base::CallbackToBlock(run_loop.QuitClosure())];

  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(1U, [runner() numberOfBlocksRemaining]);

  // Cancel the block before it is executed, then try to execute it by name.
  [runner() cancelBlockNamed:@"block"];
  [runner() runBlockIfNecessary:@"block"];

  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_EQ(0U, [runner() numberOfBlocksRemaining]);
}

// Tests that adding a block with the same name as an existing block will
// not override the existing one.
TEST_F(DeferredInitializationRunnerTest, TestAllowMultipleBlockWithSameName) {
  base::RunLoop run_loop0;
  [runner() enqueueBlockNamed:@"block"
                        block:base::CallbackToBlock(run_loop0.QuitClosure())];

  base::RunLoop run_loop1;
  [runner() enqueueBlockNamed:@"block"
                        block:base::CallbackToBlock(run_loop1.QuitClosure())];

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(2U, [runner() numberOfBlocksRemaining]);

  // Wait for the blocks to be run.
  run_loop0.Run();
  run_loop1.Run();

  EXPECT_TRUE(run_loop0.AnyQuitCalled());
  EXPECT_TRUE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(0U, [runner() numberOfBlocksRemaining]);
}
