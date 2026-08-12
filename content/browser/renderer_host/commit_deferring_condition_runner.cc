// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/commit_deferring_condition_runner.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "content/browser/back_forward_cache/back_forward_cache_commit_deferring_condition.h"
#include "content/browser/preloading/prerender/prerender_commit_deferring_condition.h"
#include "content/browser/preloading/prerender/prerender_no_vary_search_commit_deferring_condition.h"
#include "content/browser/preloading/prerender/prerender_no_vary_search_hint_commit_deferring_condition.h"
#include "content/browser/renderer_host/async_before_unload_commit_deferring_condition.h"
#include "content/browser/renderer_host/concurrent_navigations_commit_deferring_condition.h"
#include "content/browser/renderer_host/navigation_api_commit_deferring_condition.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/view_transition_commit_deferring_condition.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/common/content_features.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

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
  // Initial WebUI navigations shouldn't run CommitDeferringConditions.
  CHECK(!navigation_request.IsInitialWebUINavigation());
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
    // End `condition->TraceEventName()` trace event.
    TRACE_EVENT_END("navigation", perfetto::NamedTrack::FromPointer(
                                      "CommitDeferringConditionRunner", this));
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

  CHECK(!conditions_.empty(), base::NotFatalUntil::M152);
  return (*conditions_.begin()).get();
}

void CommitDeferringConditionRunner::ResumeProcessing() {
  if (is_in_will_commit_navigation_) {
    was_resumed_synchronously_ = true;
    return;
  }

  CHECK(is_deferred_, base::NotFatalUntil::M152);
  is_deferred_ = false;
  // End `condition->TraceEventName()` trace event.
  TRACE_EVENT_END("navigation", perfetto::NamedTrack::FromPointer(
                                    "CommitDeferringConditionRunner", this));
  // This is resuming from a check that resolved asynchronously. The current
  // check is always at the front of the vector so pop it and then proceed with
  // the next one.
  CHECK(!conditions_.empty(), base::NotFatalUntil::M152);
  conditions_.erase(conditions_.begin());
  ProcessConditions();
}

void CommitDeferringConditionRunner::RegisterDeferringConditions(
    NavigationRequest& navigation_request) {
  TRACE_EVENT("navigation",
              "CommitDeferringConditionRunner::RegisterDeferringConditions");
  // Initial WebUI navigations shouldn't run CommitDeferringConditions.
  CHECK(!navigation_request.IsInitialWebUINavigation());
  switch (navigation_type_) {
    case CommitDeferringCondition::NavigationType::kPrerenderedPageActivation:
      // For prerendered page activation, conditions should run before start
      // navigation.
      CHECK_LT(navigation_request.state(),
               NavigationRequest::WILL_START_NAVIGATION,
               base::NotFatalUntil::M152);
      break;
    case CommitDeferringCondition::NavigationType::kOther:
      // For other navigations, conditions should run before navigation commit,
      // which can be either a normal commit or an error page commit.
      // TODO(crbug.com/545094684): CHECK-exclusion: Convert to a CHECK once we
      // are confident it won't be triggered.
      DCHECK(navigation_request.state() ==
                 NavigationRequest::WILL_PROCESS_RESPONSE ||
             navigation_request.state() ==
                 NavigationRequest::WILL_FAIL_REQUEST ||
             navigation_request.state() == NavigationRequest::CANCELING);
      break;
  }

  // Let WebContents add deferring conditions.
  std::vector<std::unique_ptr<CommitDeferringCondition>> delegate_conditions =
      navigation_request.GetDelegate()
          ->CreateDeferringConditionsForNavigationCommit(navigation_request,
                                                         navigation_type_);
  for (auto& condition : delegate_conditions) {
    CHECK(condition, base::NotFatalUntil::M152);
    AddCondition(std::move(condition));
  }

  // If beforeunload handlers were run asynchronously (to avoid blocking the
  // navigation start), we must ensure they complete before the navigation is
  // allowed to commit.
  AddCondition(AsyncBeforeUnloadCommitDeferringCondition::MaybeCreate(
      navigation_request));

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
      NavigationAPICommitDeferringCondition::MaybeCreate(navigation_request));

  AddCondition(
      ViewTransitionCommitDeferringCondition::MaybeCreate(navigation_request));

  AddCondition(ConcurrentNavigationsCommitDeferringCondition::MaybeCreate(
      navigation_request, navigation_type_));

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
    is_in_will_commit_navigation_ = true;
    was_resumed_synchronously_ = false;

    base::WeakPtr<CommitDeferringConditionRunner> weak_self =
        weak_factory_.GetWeakPtr();
    CommitDeferringCondition::Result result =
        condition->WillCommitNavigation(std::move(resume_closure));
    // DO NOT ADD CODE before performing the `weak_self` check.
    // The previous call to `WillCommitNavigation()` may have caused the
    // destruction of the `NavigationRequest` that owns this
    // `CommitDeferringConditionRunner`.
    if (!weak_self) {
      // The runner was deleted, which indicates the navigation was cancelled.
      CHECK_NE(result, CommitDeferringCondition::Result::kProceed);
      return;
    }
    is_in_will_commit_navigation_ = false;

    switch (result) {
      case CommitDeferringCondition::Result::kDefer:
        // If the resume_closure has been called synchronously, treat it as
        // kProceed.
        is_in_will_commit_navigation_ = false;
        if (was_resumed_synchronously_) {
          break;
        }
        is_deferred_ = true;
        TRACE_EVENT_BEGIN("navigation",
                          perfetto::StaticString(condition->TraceEventName()),
                          perfetto::NamedTrack::FromPointer(
                              "CommitDeferringConditionRunner", this));
        return;
      case CommitDeferringCondition::Result::kCancelled:
        return;
      case CommitDeferringCondition::Result::kProceed:
        is_in_will_commit_navigation_ = false;
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
