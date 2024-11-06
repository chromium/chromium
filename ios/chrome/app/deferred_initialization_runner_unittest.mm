// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/deferred_initialization_runner.h"

#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "ios/chrome/app/deferred_initialization_queue.h"
#import "testing/platform_test.h"

class DeferredInitializationRunnerTest : public PlatformTest {
 public:
  DeferredInitializationRunnerTest() {
    _queue = [[DeferredInitializationQueue alloc]
        initWithDelayBetweenBlocks:base::Milliseconds(10)
             delayBeforeFirstBlock:base::Milliseconds(10)];

    _runner1 = [[DeferredInitializationRunner alloc] initWithQueue:_queue];
    _runner2 = [[DeferredInitializationRunner alloc] initWithQueue:_queue];
  }

  DeferredInitializationQueue* queue() { return _queue; }

  DeferredInitializationRunner* runner1() { return _runner1; }

  DeferredInitializationRunner* runner2() { return _runner2; }

 private:
  base::test::TaskEnvironment _task_environment;
  DeferredInitializationQueue* _queue;
  DeferredInitializationRunner* _runner1;
  DeferredInitializationRunner* _runner2;
};

// Tests that all blocks added on the queue are executed after a delay.
TEST_F(DeferredInitializationRunnerTest, TestRunBlockSequentially) {
  base::RunLoop run_loop0;
  [runner1() enqueueBlockNamed:@"block0"
                         block:base::CallbackToBlock(run_loop0.QuitClosure())];

  base::RunLoop run_loop1;
  [runner1() enqueueBlockNamed:@"block1"
                         block:base::CallbackToBlock(run_loop1.QuitClosure())];

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(2U, [queue() length]);

  // Wait for the blocks to be executed.
  run_loop0.Run();
  run_loop1.Run();

  EXPECT_TRUE(run_loop0.AnyQuitCalled());
  EXPECT_TRUE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(0U, [queue() length]);
}

// Tests that runBlockIfNecessary does not execute the block if it has already
// been executed and runs synchronously the one not executed.
TEST_F(DeferredInitializationRunnerTest, TestRunBlockNamed) {
  base::RunLoop run_loop0;
  [runner1() enqueueBlockNamed:@"block0"
                         block:base::CallbackToBlock(run_loop0.QuitClosure())];

  base::RunLoop run_loop1;
  [runner1() enqueueBlockNamed:@"block1"
                         block:base::CallbackToBlock(run_loop1.QuitClosure())];

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(2U, [queue() length]);

  // Wait for the first block to be executed.
  run_loop0.Run();

  EXPECT_TRUE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(1U, [queue() length]);

  // Manually invoke the blocks.
  [runner1() runBlockNamed:@"block0"];
  [runner1() runBlockNamed:@"block1"];

  EXPECT_TRUE(run_loop0.AnyQuitCalled());
  EXPECT_TRUE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(0U, [queue() length]);
}

// Tests that a block is not executed when cancelled and it is removed from the
// remaining blocks list.
TEST_F(DeferredInitializationRunnerTest, TestCancelAllBlocks) {
  base::RunLoop run_loop0;
  [runner1() enqueueBlockNamed:@"block0"
                         block:base::CallbackToBlock(run_loop0.QuitClosure())];

  base::RunLoop run_loop1;
  [runner1() enqueueBlockNamed:@"block1"
                         block:base::CallbackToBlock(run_loop1.QuitClosure())];

  base::RunLoop run_loop2;
  [runner2() enqueueBlockNamed:@"block2"
                         block:base::CallbackToBlock(run_loop2.QuitClosure())];

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_FALSE(run_loop2.AnyQuitCalled());
  EXPECT_EQ(3U, [queue() length]);

  [runner1() cancelAllBlocks];

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_FALSE(run_loop2.AnyQuitCalled());
  EXPECT_EQ(1U, [queue() length]);

  // Ensure that the blocks from the second runner are not cancelled.
  run_loop2.Run();

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_TRUE(run_loop2.AnyQuitCalled());
  EXPECT_EQ(0U, [queue() length]);
}

// Tests that cancelling blocks with context does nothing if there are no
// blocks registered .
TEST_F(DeferredInitializationRunnerTest, TestCancelAllBlocks_Empty) {
  [runner1() cancelAllBlocks];
}

// Tests that adding a block with the same name as an existing block will
// override the existing one.
TEST_F(DeferredInitializationRunnerTest, TestSecondBlockInvalidatesFirst) {
  base::RunLoop run_loop0;
  [runner1() enqueueBlockNamed:@"block"
                         block:base::CallbackToBlock(run_loop0.QuitClosure())];

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_EQ(1U, [queue() length]);

  base::RunLoop run_loop1;
  [runner1() enqueueBlockNamed:@"block"
                         block:base::CallbackToBlock(run_loop1.QuitClosure())];

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(1U, [queue() length]);

  // Wait for the block to be run.
  run_loop1.Run();

  EXPECT_FALSE(run_loop0.AnyQuitCalled());
  EXPECT_TRUE(run_loop1.AnyQuitCalled());
  EXPECT_EQ(0U, [queue() length]);
}
