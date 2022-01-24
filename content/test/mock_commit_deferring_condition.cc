// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_commit_deferring_condition.h"

#include "content/browser/renderer_host/navigation_request.h"

namespace content {

MockCommitDeferringConditionWrapper::MockCommitDeferringConditionWrapper(
    bool is_ready_to_commit) {
  condition_ = std::make_unique<MockCommitDeferringCondition>(
      is_ready_to_commit,
      base::BindOnce(
          &MockCommitDeferringConditionWrapper::WillCommitNavigationCalled,
          weak_factory_.GetWeakPtr()));
  weak_condition_ = condition_->AsWeakPtr();
}

MockCommitDeferringConditionWrapper::~MockCommitDeferringConditionWrapper() =
    default;

std::unique_ptr<MockCommitDeferringCondition>
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

void MockCommitDeferringConditionWrapper::WillCommitNavigationCalled(
    base::OnceClosure resume_closure) {
  did_call_will_commit_navigation_ = true;
  resume_closure_ = std::move(resume_closure);
  if (invoked_closure_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(invoked_closure_));
  }
}

MockCommitDeferringCondition::MockCommitDeferringCondition(
    bool is_ready_to_commit,
    WillCommitCallback on_will_commit_navigation)
    : is_ready_to_commit_(is_ready_to_commit),
      on_will_commit_navigation_(std::move(on_will_commit_navigation)) {}

MockCommitDeferringCondition::~MockCommitDeferringCondition() = default;

CommitDeferringCondition::Result
MockCommitDeferringCondition::WillCommitNavigation(base::OnceClosure resume) {
  if (on_will_commit_navigation_)
    std::move(on_will_commit_navigation_).Run(std::move(resume));
  return is_ready_to_commit_ ? Result::kProceed : Result::kDefer;
}

base::WeakPtr<MockCommitDeferringCondition>
MockCommitDeferringCondition::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

MockCommitDeferringConditionInstaller::MockCommitDeferringConditionInstaller(
    std::unique_ptr<MockCommitDeferringCondition> condition)
    : generator_id_(
          CommitDeferringConditionRunner::InstallConditionGeneratorForTesting(
              base::BindRepeating(
                  &MockCommitDeferringConditionInstaller::Install,
                  base::Unretained(this)))),
      condition_(std::move(condition)) {}

MockCommitDeferringConditionInstaller::
    ~MockCommitDeferringConditionInstaller() {
  CommitDeferringConditionRunner::UninstallConditionGeneratorForTesting(
      generator_id_);
}

std::unique_ptr<CommitDeferringCondition>
MockCommitDeferringConditionInstaller::Install() {
  return std::move(condition_);
}

}  //  namespace content
