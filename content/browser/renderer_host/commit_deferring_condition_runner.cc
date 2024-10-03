// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/commit_deferring_condition_runner.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "content/browser/preloading/prerender/prerender_commit_deferring_condition.h"
#include "content/browser/preloading/prerender/prerender_no_vary_search_commit_deferring_condition.h"
#include "content/browser/preloading/prerender/prerender_no_vary_search_hint_commit_deferring_condition.h"
#include "content/browser/renderer_host/back_forward_cache_commit_deferring_condition.h"
#include "content/browser/renderer_host/concurrent_navigations_commit_deferring_condition.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/view_transition_commit_deferring_condition.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/commit_deferring_condition.h"

namespace content {

namespace {

using GeneratorOrderPair =
    std::pair<CommitDeferringConditionRunner::ConditionGenerator,
              CommitDeferringConditionRunner::InsertOrder>;

std::map<int, GeneratorOrderPair>& GetConditionGenerators() {
  static base::NoDestructor<std::map<int, GeneratorOrderPair>> generators;
  return *generators;
}

}  // namespace

// static
std::unique_ptr<CommitDeferringConditionRunner>
CommitDeferringConditionRunner::Create(
    NavigationRequest& navigation_request,
    CommitDeferringCondition::NavigationType navigation_type,
    std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id) {
  auto runner = base::WrapUnique(new CommitDeferringConditionRunner(
      navigation_request, navigation_type,
      candidate_prerender_frame_tree_node_id));
  return runner;
}

CommitDeferringConditionRunner::CommitDeferringConditionRunner(
    Delegate& delegate,
    CommitDeferringCondition::NavigationType navigation_type,
    std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id)
    : delegate_(delegate),
      navigation_type_(navigation_type),
      candidate_prerender_frame_tree_node_id_(
          candidate_prerender_frame_tree_node_id) {}

CommitDeferringConditionRunner::~CommitDeferringConditionRunner() {
  if (is_deferred_) {
    // Pass a nullptr and it will close the opening slice.
    TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", nullptr,
                                    TRACE_ID_LOCAL(this));
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "navigation", "CommitDeferringConditionRunning", TRACE_ID_LOCAL(this));
  }
}

void CommitDeferringConditionRunner::ProcessChecks() {
  ProcessConditions();
}

void CommitDeferringConditionRunner::AddConditionForTesting(
    std::unique_ptr<CommitDeferringCondition> condition) {
  AddCondition(std::move(condition));
}

CommitDeferringCondition*
CommitDeferringConditionRunner::GetDeferringConditionForTesting() const {
  if (!is_deferred_) {
    return nullptr;
  }

  DCHECK(!conditions_.empty());
  return (*conditions_.begin()).get();
}

void CommitDeferringConditionRunner::ResumeProcessing() {
  DCHECK(is_deferred_);
  is_deferred_ = false;
  // Pass a nullptr and it will close the opening slice.
  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", nullptr, TRACE_ID_LOCAL(this));
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "navigation", "CommitDeferringConditionRunning", TRACE_ID_LOCAL(this));
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
          ->CreateDeferringConditionsForNavigationCommit(navigation_request,
                                                         navigation_type_);
  for (auto& condition : delegate_conditions) {
    DCHECK(condition);
    AddCondition(std::move(condition));
  }

  // PrerenderNoVarySearchHintCommitDeferringCondition should run before
  // PrerenderCommitDeferringCondition as it needs to defer until headers
  // are received. Headers are a required prerequisite for the correctness of
  // PrerenderCommitDeferringCondition and
  // PrerenderNoVarySearchCommitDeferringCondition in the presence of
  // No-Vary-Search hint/header.
  AddCondition(PrerenderNoVarySearchHintCommitDeferringCondition::MaybeCreate(
      navigation_request, navigation_type_,
      candidate_prerender_frame_tree_node_id_));

  AddCondition(PrerenderCommitDeferringCondition::MaybeCreate(
      navigation_request, navigation_type_,
      candidate_prerender_frame_tree_node_id_));

  // PrerenderNoVarySearchCommitDeferringCondition should run after we've
  // made the decision to activate the prerender as it changes the
  // prerender renderer's URL.
  AddCondition(PrerenderNoVarySearchCommitDeferringCondition::MaybeCreate(
      navigation_request, navigation_type_,
      candidate_prerender_frame_tree_node_id_));

  AddCondition(
      ViewTransitionCommitDeferringCondition::MaybeCreate(navigation_request));

  if (ShouldAvoidRedundantNavigationCancellations()) {
    AddCondition(ConcurrentNavigationsCommitDeferringCondition::MaybeCreate(
        navigation_request, navigation_type_));
  }

  // The BFCache deferring condition should run after all other conditions
  // since it'll disable eviction on a cached renderer.
  AddCondition(BackForwardCacheCommitDeferringCondition::MaybeCreate(
      navigation_request));

  // Run condition generators for testing.
  for (auto& iter : GetConditionGenerators()) {
    GeneratorOrderPair& generator_order_pair = iter.second;
    AddCondition(
        generator_order_pair.first.Run(navigation_request, navigation_type_),
        generator_order_pair.second);
  }
}

// static
int CommitDeferringConditionRunner::InstallConditionGeneratorForTesting(
    ConditionGenerator generator,
    InsertOrder order) {
  static int generator_id = 0;
  GetConditionGenerators().emplace(generator_id,
                                   std::make_pair(std::move(generator), order));
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
    is_deferred_ = false;
    switch (condition->WillCommitNavigation(std::move(resume_closure))) {
      case CommitDeferringCondition::Result::kDefer:
        is_deferred_ = true;
        TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("navigation",
                                          "CommitDeferringConditionRunning",
                                          TRACE_ID_LOCAL(this));
        TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
            "navigation", condition->TraceEventName(), TRACE_ID_LOCAL(this));
        return;
      // TODO(crbug.com/40270812): Also add instant tracing for the condition
      // that is being resolved synchronously.
      case CommitDeferringCondition::Result::kCancelled:
        // DO NOT ADD CODE after this. The previous call to
        // `WillCommitNavigation()` may have caused the destruction of the
        // `NavigationRequest` that owns this `CommitDeferringConditionRunner`.
        return;
      case CommitDeferringCondition::Result::kProceed:
        break;
    }

    // Otherwise, the condition is resolved synchronously so remove it and move
    // on to the next one.
    conditions_.erase(conditions_.begin());
  }

  // All checks are completed, proceed with the commit in the
  // NavigationRequest.
  delegate_->OnCommitDeferringConditionChecksComplete(
      navigation_type_, candidate_prerender_frame_tree_node_id_);
}

void CommitDeferringConditionRunner::AddCondition(
    std::unique_ptr<CommitDeferringCondition> condition,
    InsertOrder order) {
  if (!condition)
    return;

  if (order == InsertOrder::kAfter)
    conditions_.push_back(std::move(condition));
  else
    conditions_.insert(conditions_.begin(), std::move(condition));
}

}  // namespace content
