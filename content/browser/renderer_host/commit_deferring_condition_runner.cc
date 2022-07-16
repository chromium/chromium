// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/commit_deferring_condition_runner.h"

#include "content/browser/renderer_host/back_forward_cache_commit_deferring_condition.h"
#include "content/browser/renderer_host/commit_deferring_condition.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator_delegate.h"

namespace content {

namespace {

std::map<int, CommitDeferringConditionRunner::ConditionGenerator>&
GetConditionGenerators() {
  static base::NoDestructor<
      std::map<int, CommitDeferringConditionRunner::ConditionGenerator>>
      generators;
  return *generators;
}

}  // namespace

// static
std::unique_ptr<CommitDeferringConditionRunner>
CommitDeferringConditionRunner::Create(
    NavigationRequest& navigation_request,
    CommitDeferringCondition::NavigationType navigation_type,
    absl::optional<int> candidate_prerender_frame_tree_node_id) {
  auto runner = base::WrapUnique(new CommitDeferringConditionRunner(
      navigation_request, navigation_type,
      candidate_prerender_frame_tree_node_id));
  return runner;
}

CommitDeferringConditionRunner::CommitDeferringConditionRunner(
    Delegate& delegate,
    CommitDeferringCondition::NavigationType navigation_type,
    absl::optional<int> candidate_prerender_frame_tree_node_id)
    : delegate_(delegate),
      navigation_type_(navigation_type),
      candidate_prerender_frame_tree_node_id_(
          candidate_prerender_frame_tree_node_id) {}

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
  switch (navigation_type_) {
    case CommitDeferringCondition::NavigationType::kPrerenderedPageActivation:
      // For prerendered page activation, conditions should run before start
      // navigation.
      DCHECK_LT(navigation_request.state(),
                NavigationRequest::WILL_START_NAVIGATION);
      break;
    case CommitDeferringCondition::NavigationType::kOther:
      // For other navigations, conditions should run before navigation commit.
      DCHECK_EQ(navigation_request.state(),
                NavigationRequest::WILL_PROCESS_RESPONSE);
      break;
  }

  // Let WebContents add deferring conditions.
  std::vector<std::unique_ptr<CommitDeferringCondition>> delegate_conditions =
      navigation_request.GetDelegate()
          ->CreateDeferringConditionsForNavigationCommit(navigation_request);
  for (auto& condition : delegate_conditions) {
    DCHECK(condition);
    AddCondition(std::move(condition));
  }

  AddCondition(PrerenderCommitDeferringCondition::MaybeCreate(
      navigation_request, navigation_type_,
      candidate_prerender_frame_tree_node_id_));

  // The BFCache deferring condition should run after all other conditions
  // since it'll disable eviction on a cached renderer.
  AddCondition(BackForwardCacheCommitDeferringCondition::MaybeCreate(
      navigation_request));

  // Run condition generators for testing.
  for (auto& iter : GetConditionGenerators())
    AddCondition(iter.second.Run());
}

// static
int CommitDeferringConditionRunner::InstallConditionGeneratorForTesting(
    ConditionGenerator generator) {
  static int generator_id = 0;
  GetConditionGenerators().emplace(generator_id, std::move(generator));
  return generator_id++;
}

// static
void CommitDeferringConditionRunner::UninstallConditionGeneratorForTesting(
    int generator_id) {
  GetConditionGenerators().erase(generator_id);
}

void CommitDeferringConditionRunner::ProcessConditions() {
  while (!conditions_.empty()) {
    // If the condition isn't yet ready to commit, it'll be resolved
    // asynchronously. The loop will continue from ResumeProcessing();

    auto resume_closure =
        base::BindOnce(&CommitDeferringConditionRunner::ResumeProcessing,
                       weak_factory_.GetWeakPtr());
    CommitDeferringCondition* condition = (*conditions_.begin()).get();
    if (condition->WillCommitNavigation(std::move(resume_closure)) ==
        CommitDeferringCondition::Result::kDefer) {
      is_deferred_ = true;
      return;
    }

    // Otherwise, the condition is resolved synchronously so remove it and move
    // on to the next one.
    conditions_.erase(conditions_.begin());
  }

  // All checks are completed, proceed with the commit in the
  // NavigationRequest.
  delegate_.OnCommitDeferringConditionChecksComplete(
      navigation_type_, candidate_prerender_frame_tree_node_id_);
}

void CommitDeferringConditionRunner::AddCondition(
    std::unique_ptr<CommitDeferringCondition> condition) {
  if (!condition)
    return;

  conditions_.push_back(std::move(condition));
}

}  // namespace content
