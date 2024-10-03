// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/view_transition_commit_deferring_condition.h"

#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/view_transition_opt_in_state.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/frame/view_transition_state.h"

namespace content {

// static
std::unique_ptr<CommitDeferringCondition>
ViewTransitionCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kViewTransitionOnNavigation)) {
    return nullptr;
  }

  // If we already have a transition animation, we should skip the view
  // transition.
  if (navigation_request.was_initiated_by_animated_transition()) {
    return nullptr;
  }

  switch (navigation_request.frame_tree_node()->frame_tree().type()) {
    case FrameTree::Type::kPrerender:
      // Pre-rendered frame trees don't render any frames until activation. It's
      // not feasible to run transitions in a frame which has no lifecycle
      // updates.
      return nullptr;
    case FrameTree::Type::kPrimary:
      break;
    case FrameTree::Type::kFencedFrame:
      // TODO(khushalsagar): Enable for fenced frames with a WPT.
      return nullptr;
  };

  RenderFrameHostImpl* old_rfh =
      navigation_request.frame_tree_node()->current_frame_host();

  if (!navigation_request.IsInMainFrame()) {
    if (!base::FeatureList::IsEnabled(
            blink::features::kViewTransitionOnNavigationForIframes)) {
      return nullptr;
    }

    // We will not have a RFH if this navigation is not committing a new
    // Document.
    auto* new_rfh = navigation_request.GetRenderFrameHost();
    if (!new_rfh) {
      return nullptr;
    }

    // We don't have paint holding support for navigations from in-process to
    // out-of-process iframes (or vice versa). Since ViewTransition requires
    // paint holding, disable them for such navigations. This should be
    // extremely rare for same-origin navigations.
    if (old_rfh->is_local_root() != new_rfh->is_local_root()) {
      return nullptr;
    }
  }

  if (!navigation_request.ShouldDispatchPageSwapEvent()) {
    return nullptr;
  }

  if (ViewTransitionOptInState::GetOrCreateForCurrentDocument(old_rfh)
          ->same_origin_opt_in() ==
      blink::mojom::ViewTransitionSameOriginOptIn::kDisabled) {
    return nullptr;
  }

  if (navigation_request.did_encounter_cross_origin_redirect()) {
    return nullptr;
  }

  const url::Origin& current_request_origin = old_rfh->GetLastCommittedOrigin();
  const url::Origin& new_request_origin =
      navigation_request.is_running_potential_prerender_activation_checks()
          ? navigation_request.GetTentativeOriginAtRequestTime()
          : *navigation_request.GetOriginToCommit();
  if (current_request_origin != new_request_origin) {
    return nullptr;
  }

  // https://drafts.csswg.org/css-view-transitions-2/#valdef-view-transition-navigation-auto
  // `auto` is currently the only value and corresponds to enabling the boolean
  // opt in.
  switch (navigation_request.common_params().navigation_type) {
    case blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT:
    // Note: RESTORE is used for history traversals after a session restore so
    // treat these as history traversal. The initial restore itself has no
    // outgoing page so won't reach here.
    case blink::mojom::NavigationType::RESTORE:
    case blink::mojom::NavigationType::RESTORE_WITH_POST:
      break;
    case blink::mojom::NavigationType::DIFFERENT_DOCUMENT:
      if (navigation_request.browser_initiated()) {
        return nullptr;
      }
      break;
    case blink::mojom::NavigationType::RELOAD:
    case blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE:
      return nullptr;
    case blink::mojom::NavigationType::SAME_DOCUMENT:
    case blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT:
      // Same document navigations should already be excluded by
      // `ShouldDispatchPageSwapEvent`.
      NOTREACHED_IN_MIGRATION();
  }

  return base::WrapUnique(
      new ViewTransitionCommitDeferringCondition(navigation_request));
}

ViewTransitionCommitDeferringCondition::ViewTransitionCommitDeferringCondition(
    NavigationRequest& navigation_request)
    : CommitDeferringCondition(navigation_request), weak_factory_(this) {}

ViewTransitionCommitDeferringCondition::
    ~ViewTransitionCommitDeferringCondition() {
  // If we cached a view transition for the old Document and the navigation
  // has been aborted, inform the old Document to discard the pending
  // ViewTransition.
  //
  // Note: If we don't have `resources_`, they have been transferred to the
  // NavigationRequest which is now responsible to discard the old transition if
  // the navigation is cancelled.
  if (!resources_ || !old_rfh_ || !old_rfh_->IsRenderFrameLive()) {
    return;
  }

  old_rfh_->GetAssociatedLocalFrame()
      ->NotifyViewTransitionAbortedToOldDocument();
}

CommitDeferringCondition::Result
ViewTransitionCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  auto* navigation_request = NavigationRequest::From(&GetNavigationHandle());
  auto* render_frame_host =
      navigation_request->frame_tree_node()->current_frame_host();

  blink::mojom::PageSwapEventParamsPtr page_swap_event_params =
      navigation_request->WillDispatchPageSwap();
  CHECK(page_swap_event_params);

  blink::ViewTransitionToken transition_token;
  resources_ =
      std::make_unique<ScopedViewTransitionResources>(transition_token);
  resume_navigation_ = std::move(resume);
  old_rfh_ = render_frame_host->GetWeakPtr();

  CHECK(render_frame_host->IsRenderFrameLive());

  // Request a snapshot. This includes running any associaged script in the
  // renderer process.
  render_frame_host->GetAssociatedLocalFrame()
      ->SnapshotDocumentForViewTransition(
          transition_token, std::move(page_swap_event_params),
          base::BindOnce(&ViewTransitionCommitDeferringCondition::
                             OnSnapshotAckFromRenderer,
                         weak_factory_.GetWeakPtr()));

  // Also post a timeout task to resume even if the renderer has not acked.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ViewTransitionCommitDeferringCondition::OnSnapshotTimeout,
                     weak_factory_.GetWeakPtr()),
      GetSnapshotCallbackTimeout());

  return Result::kDefer;
}

const char* ViewTransitionCommitDeferringCondition::TraceEventName() const {
  return "ViewTransitionCommitDeferringCondition";
}

void ViewTransitionCommitDeferringCondition::OnSnapshotTimeout() {
  if (resume_navigation_) {
    std::move(resume_navigation_).Run();
  }
}

base::TimeDelta
ViewTransitionCommitDeferringCondition::GetSnapshotCallbackTimeout() const {
  // TODO(vmpstr): Figure out if we need to increase this in tests.
  return base::Seconds(4);
}

void ViewTransitionCommitDeferringCondition::OnSnapshotAckFromRenderer(
    const blink::ViewTransitionState& view_transition_state) {
  // The timeout may have been triggered already.
  if (!resume_navigation_) {
    return;
  }

  base::ScopedClosureRunner runner(std::move(resume_navigation_));

  if (view_transition_state.HasSubframeSnapshot()) {
    if (!old_rfh_) {
      return;
    }

    // The subframe snapshot is only used for in-process iframes which don't own
    // a widget.
    if (old_rfh_->is_local_root()) {
      return;
    }

    auto* new_rfh =
        NavigationRequest::From(&GetNavigationHandle())->GetRenderFrameHost();

    // We shouldn't send a snapshot request unless the new RFH is also an
    // in-process subframe.
    CHECK(!new_rfh->is_local_root());
    CHECK(!old_rfh_->is_main_frame()) << "Main frames must be local roots";
    CHECK(!new_rfh->is_main_frame()) << "Main frames must be local roots";
    CHECK_EQ(old_rfh_->GetProcess(), new_rfh->GetProcess())
        << "Navigation between 2 non-local roots must be in the ancestor "
           "frame's process";
  }

  if (view_transition_state.IsValid()) {
    NavigationRequest::From(&GetNavigationHandle())
        ->SetViewTransitionState(std::move(resources_),
                                 std::move(view_transition_state));
  }
}

}  // namespace content
