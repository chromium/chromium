// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_no_vary_search_hint_commit_deferring_condition.h"

#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"

namespace content {

namespace {

// Returns the root prerender frame tree node associated with navigation_request
// of ongoing prerender activation.
FrameTreeNode* GetRootPrerenderFrameTreeNode(int prerender_frame_tree_node_id) {
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
PrerenderNoVarySearchHintCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request,
    NavigationType navigation_type,
    std::optional<int> candidate_prerender_frame_tree_node_id) {
  // Don't create if No-Vary-Search support for prerender is not enabled.
  if (!base::FeatureList::IsEnabled(blink::features::kPrerender2NoVarySearch)) {
    return nullptr;
  }

  // Don't create if this navigation is not for prerender page activation.
  if (navigation_type != NavigationType::kPrerenderedPageActivation) {
    return nullptr;
  }

  // For `navigation_type` == `NavigationType::kPrerenderedPageActivation`
  // `candidate_prerender_frame_tree_node_id` has always a value as we are
  // trying to activate a prerender.
  CHECK(candidate_prerender_frame_tree_node_id.has_value());

  // Don't create if associated PrerenderHost has already received headers.
  FrameTreeNode* prerender_frame_tree_node = GetRootPrerenderFrameTreeNode(
      candidate_prerender_frame_tree_node_id.value());
  // The prerender FrameTreeNode should be here.
  CHECK(prerender_frame_tree_node);
  PrerenderHost& prerender_host =
      PrerenderHost::GetFromFrameTreeNode(*prerender_frame_tree_node);
  if (prerender_host.were_headers_received()) {
    return nullptr;
  }
  if (!prerender_host.no_vary_search_expected().has_value()) {
    return nullptr;
  }

  return base::WrapUnique(new PrerenderNoVarySearchHintCommitDeferringCondition(
      navigation_request, candidate_prerender_frame_tree_node_id.value()));
}

PrerenderNoVarySearchHintCommitDeferringCondition::
    ~PrerenderNoVarySearchHintCommitDeferringCondition() {
  // Stop observing the associated PrerenderHost to avoid use-after-free.
  FrameTreeNode* prerender_frame_tree_node =
      GetRootPrerenderFrameTreeNode(candidate_prerender_frame_tree_node_id_);
  if (!prerender_frame_tree_node) {
    // In this case the commit deferring condition was removed as an
    // observer by the PrerenderHost destructor.
    return;
  }
  PrerenderHost& prerender_host =
      PrerenderHost::GetFromFrameTreeNode(*prerender_frame_tree_node);
  prerender_host.RemoveObserver(this);
}

PrerenderNoVarySearchHintCommitDeferringCondition::
    PrerenderNoVarySearchHintCommitDeferringCondition(
        NavigationRequest& navigation_request,
        int candidate_prerender_frame_tree_node_id)
    : CommitDeferringCondition(navigation_request),
      candidate_prerender_frame_tree_node_id_(
          candidate_prerender_frame_tree_node_id) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kPrerender2NoVarySearch));
  CHECK_NE(candidate_prerender_frame_tree_node_id_,
           RenderFrameHost::kNoFrameTreeNodeId);
  FrameTreeNode* prerender_frame_tree_node =
      GetRootPrerenderFrameTreeNode(candidate_prerender_frame_tree_node_id_);
  PrerenderHost& prerender_host =
      PrerenderHost::GetFromFrameTreeNode(*prerender_frame_tree_node);
  // Add this commit deferring condition as an observer of the associated
  // PrerenderHost.
  prerender_host.AddObserver(this);
}

CommitDeferringCondition::Result
PrerenderNoVarySearchHintCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  FrameTreeNode* prerender_frame_tree_node =
      GetRootPrerenderFrameTreeNode(candidate_prerender_frame_tree_node_id_);
  // If the prerender FrameTreeNode is gone, the prerender activation is allowed
  // to continue here but will fail soon.
  if (!prerender_frame_tree_node) {
    return Result::kProceed;
  }
  // If headers were already received, proceed.
  PrerenderHost& prerender_host =
      PrerenderHost::GetFromFrameTreeNode(*prerender_frame_tree_node);
  if (prerender_host.were_headers_received()) {
    return Result::kProceed;
  }

  // `resume` callback is always set by
  // `CommitDeferringConditionRunner::ProcessConditions`.
  CHECK(resume);
  // We now need to wait for headers.
  resume_ = std::move(resume);
  return Result::kDefer;
}

void PrerenderNoVarySearchHintCommitDeferringCondition::OnHeadersReceived() {
  // Verify all conditions are met:
  // * headers should have been received and
  // * the prerender_frame_tree_node is still alive.
  FrameTreeNode* prerender_frame_tree_node =
      GetRootPrerenderFrameTreeNode(candidate_prerender_frame_tree_node_id_);
  CHECK(prerender_frame_tree_node);
  PrerenderHost& prerender_host =
      PrerenderHost::GetFromFrameTreeNode(*prerender_frame_tree_node);
  CHECK(prerender_host.were_headers_received());

  // Remove the observer from the prerender host.
  prerender_host.RemoveObserver(this);

  // Resume the navigation once the prerender main page has received the
  // headers.
  CHECK(resume_);
  std::move(resume_).Run();

  // Don't add any more code as "this" is destroyed by calling the
  // resume_ callback.
}

void PrerenderNoVarySearchHintCommitDeferringCondition::OnHostDestroyed(
    PrerenderFinalStatus status) {
  FrameTreeNode* prerender_frame_tree_node =
      GetRootPrerenderFrameTreeNode(candidate_prerender_frame_tree_node_id_);
  CHECK(prerender_frame_tree_node);
  PrerenderHost& prerender_host =
      PrerenderHost::GetFromFrameTreeNode(*prerender_frame_tree_node);

  // Remove the observer from the prerender host.
  prerender_host.RemoveObserver(this);

  // If we have not resumed the navigation, do it now. This could happen if the
  // prerender is cancelled before receiving headers.
  if (resume_) {
    std::move(resume_).Run();
  }

  // Don't add any more code as "this" is destroyed by calling the
  // resume_ callback.
}

}  // namespace content
