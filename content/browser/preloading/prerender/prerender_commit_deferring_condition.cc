// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_commit_deferring_condition.h"

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

namespace {

// Returns the root prerender frame tree node associated with navigation_request
// of ongoing prerender activation.
FrameTreeNode* GetRootPrerenderFrameTreeNode(
    FrameTreeNodeId prerender_frame_tree_node_id) {
  FrameTreeNode* root =
      FrameTreeNode::GloballyFindByID(prerender_frame_tree_node_id);
  if (root) {
    CHECK(root->IsOutermostMainFrame());
  }
  return root;
}

}  // namespace

// static
std::unique_ptr<CommitDeferringCondition>
PrerenderCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request,
    NavigationType navigation_type,
    std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id) {
  // Don't create if this navigation is not for prerender page activation.
  if (navigation_type != NavigationType::kPrerenderedPageActivation)
    return nullptr;

  return base::WrapUnique(new PrerenderCommitDeferringCondition(
      navigation_request, candidate_prerender_frame_tree_node_id.value()));
}

PrerenderCommitDeferringCondition::~PrerenderCommitDeferringCondition() =
    default;

PrerenderCommitDeferringCondition::PrerenderCommitDeferringCondition(
    NavigationRequest& navigation_request,
    FrameTreeNodeId candidate_prerender_frame_tree_node_id)
    : CommitDeferringCondition(navigation_request),
      WebContentsObserver(navigation_request.GetWebContents()),
      candidate_prerender_frame_tree_node_id_(
          candidate_prerender_frame_tree_node_id) {
  CHECK(candidate_prerender_frame_tree_node_id_);
}

CommitDeferringCondition::Result
PrerenderCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  FrameTreeNode* prerender_frame_tree_node =
      GetRootPrerenderFrameTreeNode(candidate_prerender_frame_tree_node_id_);

  // If the prerender FrameTreeNode is gone, the prerender activation is allowed
  // to continue here but will fail soon.
  if (!prerender_frame_tree_node) {
    return Result::kProceed;
  }

  PrerenderHost& prerender_host =
      PrerenderHost::GetFromFrameTreeNode(*prerender_frame_tree_node);
  // If there is no ongoing main frame navigation in prerender frame tree, the
  // prerender activation is allowed to continue.
  if (!prerender_frame_tree_node->HasNavigation()) {
    // Record the defer waiting time for PrerenderCommitDeferringCondition as no
    // delay.
    RecordPrerenderActivationCommitDeferTime(
        base::TimeDelta(), prerender_host.trigger_type(),
        prerender_host.embedder_histogram_suffix());
    return Result::kProceed;
  }

  // If we cannot match the navigation URL the prerender activation is allowed
  // to continue here but will fail soon. This can happen when matching
  // by No-Vary-Search hint and the No-Vary-Search header doesn't agree.
  if (!prerender_host.IsUrlMatch(GetNavigationHandle().GetURL())) {
    return Result::kProceed;
  }

  // Defer the prerender activation until the ongoing prerender main frame
  // navigation commits.
  done_closure_ = std::move(resume);
  defer_start_time_ = base::TimeTicks::Now();
  return Result::kDefer;
}

const char* PrerenderCommitDeferringCondition::TraceEventName() const {
  return "PrerenderCommitDeferringCondition";
}

void PrerenderCommitDeferringCondition::DidFinishNavigation(
    NavigationHandle* handle) {
  auto* finished_navigation_request = NavigationRequest::From(handle);

  FrameTreeNode* prerender_frame_tree_node =
      GetRootPrerenderFrameTreeNode(candidate_prerender_frame_tree_node_id_);

  // If the prerender frame tree node is gone, there is nothing to do.
  if (!prerender_frame_tree_node)
    return;

  // If the finished navigation is not for the prerendering main frame,
  // ignore this event.
  if (finished_navigation_request->frame_tree_node() !=
      prerender_frame_tree_node) {
    return;
  }

  // PrerenderNavigationThrottle allows navigations after the initial commit so
  // the callback should be called after all ongoing navigations are completed.
  if (done_closure_ && !prerender_frame_tree_node->HasNavigation()) {
    // It's possible that another navigation happens after posting a task to
    // resume the activation because PrerenderNavigationThrottle allows the main
    // frame navigation in a prerendered page. In that case, prerendering is
    // cancelled and the activation falls back to network.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(done_closure_));

    // Record the defer waiting time for PrerenderCommitDeferringCondition.
    base::TimeDelta delta = base::TimeTicks::Now() - defer_start_time_;
    PrerenderHost& prerender_host =
        PrerenderHost::GetFromFrameTreeNode(*prerender_frame_tree_node);
    RecordPrerenderActivationCommitDeferTime(
        delta, prerender_host.trigger_type(),
        prerender_host.embedder_histogram_suffix());
  }
}

}  // namespace content
