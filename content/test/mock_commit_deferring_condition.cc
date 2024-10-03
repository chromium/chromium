// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_commit_deferring_condition.h"

#include "content/browser/renderer_host/navigation_request.h"

namespace content {

class MockCommitDeferringCondition : public CommitDeferringCondition {
  using WillCommitCallback = base::OnceCallback<void(base::OnceClosure)>;

 public:
  // |is_ready_to_commit| specifies whether the condition is ready to commit at
  // the time WillCommitNavigation is called. If false, the runner will block
  // asynchronously until the closure passed into WillCommitNavigation is
  // invoked. |on_will_commit_navigation_| is invoked when WillCommitNavigation
  // is called by the delegate. It will receive the |resume| callback which can
  // be used to unblock an asynchronously deferred condition.
  MockCommitDeferringCondition(NavigationHandle& handle,
                               CommitDeferringCondition::Result result,
                               WillCommitCallback on_will_commit_navigation)
      : CommitDeferringCondition(handle),
        result_(result),
        on_will_commit_navigation_(std::move(on_will_commit_navigation)) {}
  ~MockCommitDeferringCondition() override = default;

  MockCommitDeferringCondition(const MockCommitDeferringCondition&) = delete;
  MockCommitDeferringCondition& operator=(const MockCommitDeferringCondition&) =
      delete;

  Result WillCommitNavigation(base::OnceClosure resume) override {
    if (on_will_commit_navigation_) {
      std::move(on_will_commit_navigation_).Run(std::move(resume));
    }
    return result_;
  }

  const char* TraceEventName() const override {
    return "MockCommitDeferringCondition";
  }

  base::WeakPtr<MockCommitDeferringCondition> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const CommitDeferringCondition::Result result_;
  WillCommitCallback on_will_commit_navigation_;
  base::WeakPtrFactory<MockCommitDeferringCondition> weak_factory_{this};
};

MockCommitDeferringConditionWrapper::MockCommitDeferringConditionWrapper(
    NavigationHandle& handle,
    CommitDeferringCondition::Result result) {
  condition_ = std::make_unique<MockCommitDeferringCondition>(
      handle, result,
      base::BindOnce(
          &MockCommitDeferringConditionWrapper::WillCommitNavigationCalled,
          weak_factory_.GetWeakPtr()));
  weak_condition_ = condition_->AsWeakPtr();
}

MockCommitDeferringConditionWrapper::~MockCommitDeferringConditionWrapper() =
    default;

std::unique_ptr<CommitDeferringCondition>
MockCommitDeferringConditionWrapper::PassToDelegate() {
  DCHECK(condition_);
  std::unique_ptr<MockCommitDeferringCondition> ret = std::move(condition_);
  return ret;
}

void MockCommitDeferringConditionWrapper::CallResumeClosure() {
  DCHECK(WasInvoked());
  DCHECK(resume_closure_);
  std::move(resume_closure_).Run();
}

bool MockCommitDeferringConditionWrapper::WasInvoked() const {
  return did_call_will_commit_navigation_;
}

bool MockCommitDeferringConditionWrapper::IsDestroyed() const {
  return !static_cast<bool>(weak_condition_);
}

void MockCommitDeferringConditionWrapper::WaitUntilInvoked() {
  if (WasInvoked())
    return;
  base::RunLoop loop;
  invoked_closure_ = loop.QuitClosure();
  loop.Run();
}

base::OnceClosure MockCommitDeferringConditionWrapper::TakeResumeClosure() {
  DCHECK(resume_closure_);
  return std::move(resume_closure_);
}

void MockCommitDeferringConditionWrapper::WillCommitNavigationCalled(
    base::OnceClosure resume_closure) {
  did_call_will_commit_navigation_ = true;
  resume_closure_ = std::move(resume_closure);
  if (invoked_closure_)
    std::move(invoked_closure_).Run();
}

MockCommitDeferringConditionInstaller::MockCommitDeferringConditionInstaller(
    const GURL& url,
    CommitDeferringCondition::Result result,
    CommitDeferringConditionRunner::InsertOrder order)
    : url_(url),
      result_(result),
      generator_id_(
          CommitDeferringConditionRunner::InstallConditionGeneratorForTesting(
              base::BindRepeating(
                  &MockCommitDeferringConditionInstaller::Install,
                  base::Unretained(this)),
              order)) {}

MockCommitDeferringConditionInstaller::
    ~MockCommitDeferringConditionInstaller() {
  CommitDeferringConditionRunner::UninstallConditionGeneratorForTesting(
      generator_id_);
}

void MockCommitDeferringConditionInstaller::WaitUntilInstalled() {
  if (installed_condition_)
    return;
  base::RunLoop loop;
  was_installed_closure_ = loop.QuitClosure();
  loop.Run();
}

std::unique_ptr<CommitDeferringCondition>
MockCommitDeferringConditionInstaller::Install(
    NavigationHandle& handle,
    CommitDeferringCondition::NavigationType /*type*/) {
  DCHECK(!installed_condition_)
      << "MockCommitDeferringConditionInstaller can only be used on a single "
         "navigation, received second navigation to: "
      << url_;
  installed_condition_ =
      std::make_unique<MockCommitDeferringConditionWrapper>(handle, result_);
  if (was_installed_closure_)
    std::move(was_installed_closure_).Run();
  return installed_condition_->PassToDelegate();
}

}  //  namespace content
