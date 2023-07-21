// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_TEST_MOCK_COMMIT_DEFERRING_CONDITION_H_

#include "base/functional/callback.h"
#include "content/browser/renderer_host/commit_deferring_condition_runner.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {

class MockCommitDeferringCondition;

// Since a CommitDeferringCondition is owned by the delegate, test code must
// transfer ownership of the condition to the delegate. This makes it
// cumbersome to interact and inspect with the condition from test code. This
// wrapper creates and holds a weak pointer to the condition as well as some
// extra book keeping to make testing it more convenient. Since
// CommitDeferringConditions can be registered and run at different phases of a
// navigation, adding a condition at the right time can be subtle. Use
// MockCommitDeferringConditionInstaller rather than trying to manually add a
// condition to a NavigationRequest.
class MockCommitDeferringConditionWrapper {
 public:
  explicit MockCommitDeferringConditionWrapper(
      NavigationHandle& handle,
      CommitDeferringCondition::Result result);

  MockCommitDeferringConditionWrapper(
      const MockCommitDeferringConditionWrapper&) = delete;
  MockCommitDeferringConditionWrapper& operator=(
      const MockCommitDeferringConditionWrapper&) = delete;

  ~MockCommitDeferringConditionWrapper();
  std::unique_ptr<CommitDeferringCondition> PassToDelegate();
  void CallResumeClosure();
  bool WasInvoked() const;
  bool IsDestroyed() const;
  void WaitUntilInvoked();

  base::OnceClosure TakeResumeClosure();

 private:
  void WillCommitNavigationCalled(base::OnceClosure resume_closure);

  std::unique_ptr<MockCommitDeferringCondition> condition_;
  base::WeakPtr<MockCommitDeferringCondition> weak_condition_;

  base::OnceClosure resume_closure_;
  base::OnceClosure invoked_closure_;

  bool did_call_will_commit_navigation_ = false;

  base::WeakPtrFactory<MockCommitDeferringConditionWrapper> weak_factory_{this};
};

// This class will create and insert a MockCommitDeferringCondition into the
// next starting navigation. By default, the mock condition will be installed
// to run after real conditions. The installer can only be used for a single
// navigation.
class MockCommitDeferringConditionInstaller {
 public:
  explicit MockCommitDeferringConditionInstaller(
      const GURL& url,
      CommitDeferringCondition::Result result,
      CommitDeferringConditionRunner::InsertOrder order =
          CommitDeferringConditionRunner::InsertOrder::kAfter);
  ~MockCommitDeferringConditionInstaller();

  // Waits in a RunLoop until the condition has been installed into a matching
  // navigation. `condition()` can always be called after this is called.
  void WaitUntilInstalled();

  // Returns a reference to the (wrapped) condition that was installed on the
  // matching navigation. This should only be called after a condition has been
  // installed.
  MockCommitDeferringConditionWrapper& condition() {
    DCHECK(installed_condition_);
    return *installed_condition_;
  }

 private:
  std::unique_ptr<CommitDeferringCondition> Install(
      NavigationHandle& handle,
      CommitDeferringCondition::NavigationType type);

  GURL url_;
  CommitDeferringCondition::Result result_;
  const int generator_id_;

  base::OnceClosure was_installed_closure_;

  std::unique_ptr<MockCommitDeferringConditionWrapper> installed_condition_;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_COMMIT_DEFERRING_CONDITION_H_
