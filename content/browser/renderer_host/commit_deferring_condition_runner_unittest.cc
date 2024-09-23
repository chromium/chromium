// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/commit_deferring_condition_runner.h"

#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/test/mock_navigation_handle.h"
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
    runner_ = base::WrapUnique(new CommitDeferringConditionRunner(
        *this, CommitDeferringCondition::NavigationType::kOther,
        /*candidate_prerender_frame_tree_node_id=*/std::nullopt));
  }

  // Whether the callback was called.
  bool was_delegate_notified() const { return was_delegate_notified_; }

  bool is_deferring() { return runner_->is_deferred_; }

  CommitDeferringConditionRunner* runner() { return runner_.get(); }

 private:
  // CommitDeferringConditionRunner::Delegate:
  void OnCommitDeferringConditionChecksComplete(
      CommitDeferringCondition::NavigationType navigation_type,
      std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id)
      override {
    EXPECT_EQ(navigation_type,
              CommitDeferringCondition::NavigationType::kOther);
    EXPECT_FALSE(candidate_prerender_frame_tree_node_id.has_value());
    was_delegate_notified_ = true;
  }

  std::unique_ptr<CommitDeferringConditionRunner> runner_;
  bool was_delegate_notified_ = false;
};

// CommitDeferringCondition always need a NavigationHandle. Since we don't have
// a navigation here, this class is just used to provide it with a
// MockNavigationHandle.
class MockHandleConditionWrapper : public MockNavigationHandle,
                                   public MockCommitDeferringConditionWrapper {
 public:
  explicit MockHandleConditionWrapper(CommitDeferringCondition::Result result)
      : MockCommitDeferringConditionWrapper(*this, result) {}
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
  MockHandleConditionWrapper condition(
      CommitDeferringCondition::Result::kDefer);
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
  MockHandleConditionWrapper condition(
      CommitDeferringCondition::Result::kProceed);
  runner()->AddConditionForTesting(condition.PassToDelegate());
  runner()->ProcessChecks();
  EXPECT_TRUE(was_delegate_notified());
  EXPECT_TRUE(condition.WasInvoked());
}

// Test that if a condition indicating the cancellation of the commit,
// the delegate is not notified.
TEST_F(CommitDeferringConditionRunnerTest, BasicCancelled) {
  MockHandleConditionWrapper condition(
      CommitDeferringCondition::Result::kCancelled);
  runner()->AddConditionForTesting(condition.PassToDelegate());
  runner()->ProcessChecks();
  EXPECT_FALSE(was_delegate_notified());
  EXPECT_TRUE(condition.WasInvoked());
}

// Test with multiple conditions, some of which are completed synchronously and
// some asynchronously. The final condition is asynchronous and should notify
// the delegate on resumption.
TEST_F(CommitDeferringConditionRunnerTest, MultipleConditionsLastAsync) {
  // Add conditions, alternating between those that are already satisfied at
  // ProcessChecks time and those that complete asynchronously.
  // Complete -> Async -> Complete -> Async
  MockHandleConditionWrapper condition1(
      CommitDeferringCondition::Result::kProceed);
  runner()->AddConditionForTesting(condition1.PassToDelegate());

  MockHandleConditionWrapper condition2(
      CommitDeferringCondition::Result::kDefer);
  runner()->AddConditionForTesting(condition2.PassToDelegate());

  MockHandleConditionWrapper condition3(
      CommitDeferringCondition::Result::kProceed);
  runner()->AddConditionForTesting(condition3.PassToDelegate());

  MockHandleConditionWrapper condition4(
      CommitDeferringCondition::Result::kDefer);
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
  MockHandleConditionWrapper condition1(
      CommitDeferringCondition::Result::kDefer);
  runner()->AddConditionForTesting(condition1.PassToDelegate());

  MockHandleConditionWrapper condition2(
      CommitDeferringCondition::Result::kProceed);
  runner()->AddConditionForTesting(condition2.PassToDelegate());

  MockHandleConditionWrapper condition3(
      CommitDeferringCondition::Result::kDefer);
  runner()->AddConditionForTesting(condition3.PassToDelegate());

  MockHandleConditionWrapper condition4(
      CommitDeferringCondition::Result::kProceed);
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

// Test with multiple conditions, with one indicating that the commit is
// cancelled invoked in the middle.
TEST_F(CommitDeferringConditionRunnerTest, MultipleConditionsWithCancelled) {
  MockHandleConditionWrapper condition1(
      CommitDeferringCondition::Result::kProceed);
  runner()->AddConditionForTesting(condition1.PassToDelegate());

  MockHandleConditionWrapper condition2(
      CommitDeferringCondition::Result::kCancelled);
  runner()->AddConditionForTesting(condition2.PassToDelegate());

  MockHandleConditionWrapper condition3(
      CommitDeferringCondition::Result::kProceed);
  runner()->AddConditionForTesting(condition3.PassToDelegate());

  runner()->ProcessChecks();

  // Only the first two conditions are invoked, as the commit is cancelled with
  // the second condition.
  EXPECT_FALSE(was_delegate_notified());
  EXPECT_TRUE(condition1.WasInvoked());
  EXPECT_TRUE(condition2.WasInvoked());
  EXPECT_FALSE(condition3.WasInvoked());
  EXPECT_FALSE(is_deferring());
}

}  // namespace content
