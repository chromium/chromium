// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_commit_deferring_condition.h"

#include "content/browser/prerender/prerender_host.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

// Returns the root prerender frame tree node associated with navigation_request
// of ongoing prerender activation.
FrameTreeNode* GetRootPrerenderFrameTreeNode(
    NavigationRequest& navigation_request) {
  int prerender_frame_tree_node_id =
      navigation_request.prerender_frame_tree_node_id();
  FrameTreeNode* prerender_frame_tree_node =
      FrameTreeNode::GloballyFindByID(prerender_frame_tree_node_id);
  return prerender_frame_tree_node
             ? prerender_frame_tree_node->frame_tree()->root()
             : nullptr;
}

}  // namespace

// static
std::unique_ptr<CommitDeferringCondition>
PrerenderCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request) {
  // Don't create if this navigation is not for prerender page activation.
  if (!navigation_request.IsPrerenderedPageActivation())
    return nullptr;

  return base::WrapUnique(
      new PrerenderCommitDeferringCondition(navigation_request));
}

PrerenderCommitDeferringCondition::~PrerenderCommitDeferringCondition() =
    default;

PrerenderCommitDeferringCondition::PrerenderCommitDeferringCondition(
    NavigationRequest& navigation_request)
    : WebContentsObserver(navigation_request.GetWebContents()),
      navigation_request_(navigation_request) {}

CommitDeferringCondition::Result
PrerenderCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  // If there is no ongoing main frame navigation in prerender frame tree, the
  // prerender activation is allowed to continue.
  if (!GetRootPrerenderFrameTreeNode(navigation_request_)->HasNavigation())
    return kProceed;

  // Defer the prerender activation until the ongoing prerender main frame
  // navigation commits.
  done_closure_ = std::move(resume);
  defer_start_time_ = base::TimeTicks::Now();
  return kDefer;
}

void PrerenderCommitDeferringCondition::DidFinishNavigation(
    NavigationHandle* handle) {
  auto* finished_navigation_request = NavigationRequest::From(handle);

  FrameTreeNode* prerender_frame_tree_node =
      GetRootPrerenderFrameTreeNode(navigation_request_);

  // If the prerender frame tree node is gone, there is nothing to do.
  if (!prerender_frame_tree_node)
    return;

  // If the finished navigation is not for the prerendering main frame,
  // ignore this event.
  if (finished_navigation_request->frame_tree_node() !=
      prerender_frame_tree_node) {
    return;
  }

  // Since the prerender navigation finished, and
  // PrerenderNavigationThrottle disallows another navigation after the
  // initial commit, there should not be another navigation starting.
  DCHECK(!prerender_frame_tree_node->HasNavigation());

  if (done_closure_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(done_closure_));

    // Record the defer waiting time for PrerenderCommitDeferringCondition.
    base::TimeDelta delta = base::TimeTicks::Now() - defer_start_time_;
    base::UmaHistogramTimes("Navigation.Prerender.ActivationCommitDeferTime",
                            delta);
  }
}

}  // namespace content
