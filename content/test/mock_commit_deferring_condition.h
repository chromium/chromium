// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_TEST_MOCK_COMMIT_DEFERRING_CONDITION_H_

#include "base/callback.h"
#include "content/browser/renderer_host/commit_deferring_condition.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class MockCommitDeferringCondition;

// Since a CommitDeferringCondition is owned by the delegate, test code must
// transfer ownership of the condition to the delegate. This makes it
// cumbersome to interact and inspect with the condition from test code. This
// wrapper creates and holds a weak pointer to the condition as well as some
// extra book keeping to make testing it more convenient.
class MockCommitDeferringConditionWrapper {
 public:
  explicit MockCommitDeferringConditionWrapper(bool is_ready_to_commit);

  MockCommitDeferringConditionWrapper(
      const MockCommitDeferringConditionWrapper&) = delete;
  MockCommitDeferringConditionWrapper& operator=(
      const MockCommitDeferringConditionWrapper&) = delete;

  ~MockCommitDeferringConditionWrapper();
  std::unique_ptr<MockCommitDeferringCondition> PassToDelegate();
  void CallResumeClosure();
  bool WasInvoked() const;
  bool IsDestroyed() const;
  void WaitUntilInvoked();

 private:
  void WillCommitNavigationCalled(base::OnceClosure resume_closure);

  std::unique_ptr<MockCommitDeferringCondition> condition_;
  base::WeakPtr<MockCommitDeferringCondition> weak_condition_;

  base::OnceClosure resume_closure_;
  base::OnceClosure invoked_closure_;

  bool did_call_will_commit_navigation_ = false;

  base::WeakPtrFactory<MockCommitDeferringConditionWrapper> weak_factory_{this};
};

class MockCommitDeferringCondition : public CommitDeferringCondition {
  using WillCommitCallback = base::OnceCallback<void(base::OnceClosure)>;

 public:
  // |is_ready_to_commit| specifies whether the condition is ready to commit at
  // the time WillCommitNavigation is called. If false, the runner will block
  // asynchronously until the closure passed into WillCommitNavigation is
  // invoked. |on_will_commit_navigation_| is invoked when WillCommitNavigation
  // is called by the delegate. It will receive the |resume| callback which can
  // be used to unblock an asynchronously deferred condition.
  MockCommitDeferringCondition(bool is_ready_to_commit,
                               WillCommitCallback on_will_commit_navigation);
  ~MockCommitDeferringCondition() override;
  Result WillCommitNavigation(base::OnceClosure resume) override;

  base::WeakPtr<MockCommitDeferringCondition> AsWeakPtr();

 private:
  const bool is_ready_to_commit_;
  WillCommitCallback on_will_commit_navigation_;

  base::WeakPtrFactory<MockCommitDeferringCondition> weak_factory_{this};
};

// This class will register the given CommitDeferringCondition into any starting
// navigation. The mock condition will be installed to run after real
// conditions.
class MockCommitDeferringConditionInstaller {
 public:
  explicit MockCommitDeferringConditionInstaller(
      std::unique_ptr<MockCommitDeferringCondition> condition);
  ~MockCommitDeferringConditionInstaller();

 private:
  std::unique_ptr<CommitDeferringCondition> Install();

  const int generator_id_;
  std::unique_ptr<MockCommitDeferringCondition> condition_;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_COMMIT_DEFERRING_CONDITION_H_
