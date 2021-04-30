// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_commit_deferring_condition.h"

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

void MockCommitDeferringConditionWrapper::WillCommitNavigationCalled(
    base::OnceClosure resume_closure) {
  did_call_will_commit_navigation_ = true;
  resume_closure_ = std::move(resume_closure);
}

MockCommitDeferringCondition::MockCommitDeferringCondition(
    bool is_ready_to_commit,
    WillCommitCallback on_will_commit_navigation)
    : is_ready_to_commit_(is_ready_to_commit),
      on_will_commit_navigation_(std::move(on_will_commit_navigation)) {}

MockCommitDeferringCondition::~MockCommitDeferringCondition() = default;

bool MockCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  if (on_will_commit_navigation_)
    std::move(on_will_commit_navigation_).Run(std::move(resume));
  return is_ready_to_commit_;
}

base::WeakPtr<MockCommitDeferringCondition>
MockCommitDeferringCondition::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  //  namespace content
