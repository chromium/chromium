// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_no_vary_search_commit_deferring_condition.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/browser/preloading/prerender/prerender_host.h"
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
PrerenderNoVarySearchCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request,
    NavigationType navigation_type,
    std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id) {
  // Don't create if this navigation is not for prerender page activation.
  if (navigation_type != NavigationType::kPrerenderedPageActivation) {
    return nullptr;
  }

  return base::WrapUnique(new PrerenderNoVarySearchCommitDeferringCondition(
      navigation_request, candidate_prerender_frame_tree_node_id.value()));
}

// static
void PrerenderNoVarySearchCommitDeferringCondition::OnUrlUpdated(
    base::TimeTicks defer_start_time,
    std::string histogram_suffix,
    base::OnceClosure resume) {
  // Resume the prerender activation.
  base::TimeDelta time_delta = base::TimeTicks::Now() - defer_start_time;
  base::UmaHistogramTimes(
      "Navigation.Prerender.NoVarySearchCommitDeferTime" + histogram_suffix,
      time_delta);
  std::move(resume).Run();
}

PrerenderNoVarySearchCommitDeferringCondition::
    ~PrerenderNoVarySearchCommitDeferringCondition() = default;

PrerenderNoVarySearchCommitDeferringCondition::
    PrerenderNoVarySearchCommitDeferringCondition(
        NavigationRequest& navigation_request,
        FrameTreeNodeId candidate_prerender_frame_tree_node_id)
    : CommitDeferringCondition(navigation_request),
      candidate_prerender_frame_tree_node_id_(
          candidate_prerender_frame_tree_node_id) {
  CHECK(candidate_prerender_frame_tree_node_id_);
}

CommitDeferringCondition::Result
PrerenderNoVarySearchCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  FrameTreeNode* prerender_frame_tree_node =
      GetRootPrerenderFrameTreeNode(candidate_prerender_frame_tree_node_id_);
  // If the prerender FrameTreeNode is gone, the prerender activation is allowed
  // to continue here but will fail soon.
  if (!prerender_frame_tree_node) {
    return Result::kProceed;
  }
  // If there is no need to change the URL of the prerender before using it
  // continue. Otherwise inform the prerender renderer to update its URL
  // and wait for it to finish before proceeding. This is needed because
  // the omnibox should reflect the navigation URL which is an inexact match
  // via No Vary Search to the prerender URL.
  PrerenderHost& prerender_host =
      PrerenderHost::GetFromFrameTreeNode(*prerender_frame_tree_node);
  // If we cannot match the navigation URL the prerender activation is allowed
  // to continue here but will fail soon. This can happen when matching
  // by No-Vary-Search hint and the No-Vary-Search header doesn't agree.
  if (!prerender_host.IsUrlMatch(GetNavigationHandle().GetURL())) {
    return Result::kProceed;
  }
  // If the prerender has been redirected or navigated, we do not change URL.
  // A prerendered page can navigate to another same-site URL during
  // prerendering. E.g. a prerendered page navigates from
  // URL-A-with-NoVarySearch to URL-B. The omnibox should reflect URL-B instead
  // of URL-A-with-NoVarySearch.
  if (prerender_host.GetInitialUrl() !=
      prerender_host.GetPrerenderedMainFrameHost()->GetLastCommittedURL()) {
    return Result::kProceed;
  }
  // If the prerender initial URL is the same as the navigation URL there is no
  // need to change URL.
  if (prerender_host.GetInitialUrl() == GetNavigationHandle().GetURL()) {
    return Result::kProceed;
  }
  // Inform prerender renderer to change its URL. Make sure the two URLs are
  // same origin.
  CHECK(prerender_host.GetPrerenderedMainFrameHost()
            ->GetLastCommittedOrigin()
            .IsSameOriginWith(GetNavigationHandle().GetURL()));
  prerender_host.GetPrerenderedMainFrameHost()
      ->GetAssociatedLocalFrame()
      ->UpdatePrerenderURL(
          GetNavigationHandle().GetURL(),
          base::BindOnce(
              PrerenderNoVarySearchCommitDeferringCondition::OnUrlUpdated,
              base::TimeTicks::Now(), prerender_host.GetHistogramSuffix(),
              std::move(resume)));
  // Defer the prerender activation until the ongoing prerender main frame
  // changes the URL.
  return Result::kDefer;
}

const char* PrerenderNoVarySearchCommitDeferringCondition::TraceEventName()
    const {
  return "PrerenderNoVarySearchCommitDeferringCondition";
}

}  // namespace content
