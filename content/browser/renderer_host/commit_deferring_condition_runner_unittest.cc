// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/commit_deferring_condition_runner.h"

#include "content/browser/renderer_host/commit_deferring_condition.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/mock_commit_deferring_condition.h"

namespace content {

class CommitDeferringConditionRunnerTest
    : public RenderViewHostTestHarness,
      public CommitDeferringConditionRunner::Delegate {
 public:
  CommitDeferringConditionRunnerTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    runner_ = base::WrapUnique(new CommitDeferringConditionRunner(*this));
  }

  // Whether the callback was called.
  bool was_delegate_notified() const { return was_delegate_notified_; }

  bool is_deferring() { return runner_->is_deferred_; }

  CommitDeferringConditionRunner* runner() { return runner_.get(); }

 private:
  // CommitDeferringConditionRunner::Delegate:
  void OnCommitDeferringConditionChecksComplete() override {
    was_delegate_notified_ = true;
  }

  std::unique_ptr<CommitDeferringConditionRunner> runner_;
  bool was_delegate_notified_ = false;
};

// Check that the runner notifies the delegate synchronously when there are no
// conditions registered.
TEST_F(CommitDeferringConditionRunnerTest, NoRegisteredConditions) {
  EXPECT_FALSE(was_delegate_notified());
  runner()->ProcessChecks();
  EXPECT_TRUE(was_delegate_notified());
}

// Test that when a condition defers asynchronously, the delegate isn't
// notified until the condition signals completion.
TEST_F(CommitDeferringConditionRunnerTest, BasicAsync) {
  MockCommitDeferringConditionWrapper condition(/*is_ready_to_commit=*/false);
  runner()->AddConditionForTesting(condition.PassToDelegate());
  runner()->ProcessChecks();
  EXPECT_FALSE(was_delegate_notified());
  EXPECT_TRUE(condition.WasInvoked());
  EXPECT_TRUE(is_deferring());
  condition.CallResumeClosure();
  EXPECT_TRUE(was_delegate_notified());
}

// Test that if a condition is already satisfied when ProcessChecks is
// called, the delegate is notified synchronously.
TEST_F(CommitDeferringConditionRunnerTest, BasicSync) {
  MockCommitDeferringConditionWrapper condition(/*is_ready_to_commit=*/true);
  runner()->AddConditionForTesting(condition.PassToDelegate());
  runner()->ProcessChecks();
  EXPECT_TRUE(was_delegate_notified());
  EXPECT_TRUE(condition.WasInvoked());
}

// Test with multiple conditions, some of which are completed synchronously and
// some asynchronously. The final condition is asynchronous and should notify
// the delegate on resumption.
TEST_F(CommitDeferringConditionRunnerTest, MultipleConditionsLastAsync) {
  // Add conditions, alternating between those that are already satisfied at
  // ProcessChecks time and those that complete asynchronously.
  // Complete -> Async -> Complete -> Async
  MockCommitDeferringConditionWrapper condition1(/*is_ready_to_commit=*/true);
  runner()->AddConditionForTesting(condition1.PassToDelegate());

  MockCommitDeferringConditionWrapper condition2(/*is_ready_to_commit=*/false);
  runner()->AddConditionForTesting(condition2.PassToDelegate());

  MockCommitDeferringConditionWrapper condition3(/*is_ready_to_commit=*/true);
  runner()->AddConditionForTesting(condition3.PassToDelegate());

  MockCommitDeferringConditionWrapper condition4(/*is_ready_to_commit=*/false);
  runner()->AddConditionForTesting(condition4.PassToDelegate());

  runner()->ProcessChecks();

  // The first should have been completed synchronously so we should have
  // invoked the second condition and are waiting on it now.
  EXPECT_FALSE(was_delegate_notified());
  EXPECT_TRUE(condition1.WasInvoked());
  EXPECT_TRUE(condition2.WasInvoked());
  EXPECT_FALSE(condition3.WasInvoked());
  EXPECT_FALSE(condition4.WasInvoked());
  EXPECT_TRUE(is_deferring());

  // Complete the second condition. The third is already completed so we should
  // synchronously skip to the fourth.
  condition2.CallResumeClosure();
  EXPECT_FALSE(was_delegate_notified());
  EXPECT_TRUE(condition3.WasInvoked());
  EXPECT_TRUE(condition4.WasInvoked());
  EXPECT_TRUE(is_deferring());

  // Completing the final condition should notify the delegate.
  condition4.CallResumeClosure();
  EXPECT_TRUE(was_delegate_notified());
  EXPECT_FALSE(is_deferring());
}

// Test with multiple conditions, some of which are completed synchronously and
// some asynchronously. The final condition is synchronous and should notify
// the delegate synchronously from resuming the last asynchronous condition.
TEST_F(CommitDeferringConditionRunnerTest, MultipleConditionsLastSync) {
  // Add conditions, alternating between those that are already satisfied at
  // ProcessChecks time and those that complete asynchronously.
  // Async -> Complete -> Async -> Complete
  MockCommitDeferringConditionWrapper condition1(/*is_ready_to_commit=*/false);
  runner()->AddConditionForTesting(condition1.PassToDelegate());

  MockCommitDeferringConditionWrapper condition2(/*is_ready_to_commit=*/true);
  runner()->AddConditionForTesting(condition2.PassToDelegate());

  MockCommitDeferringConditionWrapper condition3(/*is_ready_to_commit=*/false);
  runner()->AddConditionForTesting(condition3.PassToDelegate());

  MockCommitDeferringConditionWrapper condition4(/*is_ready_to_commit=*/true);
  runner()->AddConditionForTesting(condition4.PassToDelegate());

  runner()->ProcessChecks();

  // The first condition is deferring asynchronously.
  EXPECT_FALSE(was_delegate_notified());
  EXPECT_TRUE(condition1.WasInvoked());
  EXPECT_FALSE(condition2.WasInvoked());
  EXPECT_TRUE(is_deferring());

  // Complete the first condition. The second is a synchronous condition so we
  // should now be awaiting completion of the third which is async.
  condition1.CallResumeClosure();
  EXPECT_FALSE(was_delegate_notified());
  EXPECT_TRUE(condition2.WasInvoked());
  EXPECT_TRUE(condition3.WasInvoked());
  EXPECT_FALSE(condition4.WasInvoked());
  EXPECT_TRUE(is_deferring());

  // Resuming from the third condition should synchronously complete the fourth
  // and then notify the delegate.
  condition3.CallResumeClosure();
  EXPECT_TRUE(condition4.WasInvoked());
  EXPECT_TRUE(was_delegate_notified());
  EXPECT_FALSE(is_deferring());
}

}  // namespace content
