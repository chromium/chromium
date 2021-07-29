// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/commit_deferring_condition_runner.h"

#include "content/browser/renderer_host/back_forward_cache_commit_deferring_condition.h"
#include "content/browser/renderer_host/commit_deferring_condition.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator_delegate.h"

namespace content {

// static
std::unique_ptr<CommitDeferringConditionRunner>
CommitDeferringConditionRunner::Create(NavigationRequest& navigation_request) {
  auto runner =
      base::WrapUnique(new CommitDeferringConditionRunner(navigation_request));
  return runner;
}

CommitDeferringConditionRunner::CommitDeferringConditionRunner(
    Delegate& delegate)
    : delegate_(delegate) {}

CommitDeferringConditionRunner::~CommitDeferringConditionRunner() = default;

void CommitDeferringConditionRunner::ProcessChecks() {
  ProcessConditions();
}

void CommitDeferringConditionRunner::AddConditionForTesting(
    std::unique_ptr<CommitDeferringCondition> condition) {
  AddCondition(std::move(condition));
}

bool CommitDeferringConditionRunner::is_deferred_for_testing() const {
  return is_deferred_;
}

void CommitDeferringConditionRunner::ResumeProcessing() {
  DCHECK(is_deferred_);
  is_deferred_ = false;

  // This is resuming from a check that resolved asynchronously. The current
  // check is always at the front of the vector so pop it and then proceed with
  // the next one.
  DCHECK(!conditions_.empty());
  conditions_.erase(conditions_.begin());
  ProcessConditions();
}

void CommitDeferringConditionRunner::RegisterDeferringConditions(
    NavigationRequest& navigation_request) {
  // Let WebContents add deferring conditions.
  std::vector<std::unique_ptr<CommitDeferringCondition>> delegate_conditions =
      navigation_request.GetDelegate()
          ->CreateDeferringConditionsForNavigationCommit(navigation_request);
  for (auto& condition : delegate_conditions) {
    DCHECK(condition);
    AddCondition(std::move(condition));
  }

  // The BFCache deferring condition should run after all other conditions
  // since it'll disable eviction on a cached renderer.
  AddCondition(BackForwardCacheCommitDeferringCondition::MaybeCreate(
      navigation_request));
}

void CommitDeferringConditionRunner::ProcessConditions() {
  while (!conditions_.empty()) {
    // If the condition isn't yet ready to commit, it'll be resolved
    // asynchronously. The loop will continue from ResumeProcessing();

    auto resume_closure =
        base::BindOnce(&CommitDeferringConditionRunner::ResumeProcessing,
                       weak_factory_.GetWeakPtr());
    if (!(*conditions_.begin())
             ->WillCommitNavigation(std::move(resume_closure))) {
      is_deferred_ = true;
      return;
    }

    // Otherwise, the condition is resolved synchronously so remove it and move
    // on to the next one.
    conditions_.erase(conditions_.begin());
  }

  // All checks are completed, proceed with the commit in the
  // NavigationRequest.
  delegate_.OnCommitDeferringConditionChecksComplete();
}

void CommitDeferringConditionRunner::AddCondition(
    std::unique_ptr<CommitDeferringCondition> condition) {
  if (!condition)
    return;

  conditions_.push_back(std::move(condition));
}

}  // namespace content
