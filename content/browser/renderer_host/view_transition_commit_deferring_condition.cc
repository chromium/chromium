// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/view_transition_commit_deferring_condition.h"

#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/view_transition_opt_in_state.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/frame/view_transition_state.h"

namespace content {
namespace {

}  // namespace

// static
std::unique_ptr<CommitDeferringCondition>
ViewTransitionCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kViewTransitionOnNavigation)) {
    return nullptr;
  }

  if (!navigation_request.IsInPrimaryMainFrame())
    return nullptr;

  if (!navigation_request.ShouldDispatchPageSwapEvent()) {
    return nullptr;
  }

  RenderFrameHostImpl* rfh =
      navigation_request.frame_tree_node()->current_frame_host();
  if (ViewTransitionOptInState::GetOrCreateForCurrentDocument(rfh)
          ->same_origin_opt_in() ==
      blink::mojom::ViewTransitionSameOriginOptIn::kDisabled) {
    return nullptr;
  }

  const url::Origin& current_request_origin = rfh->GetLastCommittedOrigin();
  const url::Origin& new_request_origin =
      navigation_request.is_running_potential_prerender_activation_checks()
          ? navigation_request.GetTentativeOriginAtRequestTime()
          : *navigation_request.GetOriginToCommit();
  // Only support same origin.
  // TODO(khushalsagar): We need to be able to deal with redirects.
  // https://github.com/WICG/view-transitions/issues/200
  if (current_request_origin != new_request_origin) {
    return nullptr;
  }
  CHECK(!current_request_origin.opaque());

  // Per-spec, reloads are excluded from the `auto` value which sets the
  // boolean opt in. If a value specific to reloads is added, we'll need a
  // finer-grained opt-in from the renderer.
  if (navigation_request.GetReloadType() != ReloadType::NONE) {
    return nullptr;
  }

  return base::WrapUnique(
      new ViewTransitionCommitDeferringCondition(navigation_request));
}

ViewTransitionCommitDeferringCondition::ViewTransitionCommitDeferringCondition(
    NavigationRequest& navigation_request)
    : CommitDeferringCondition(navigation_request) {}

ViewTransitionCommitDeferringCondition::
    ~ViewTransitionCommitDeferringCondition() = default;

CommitDeferringCondition::Result
ViewTransitionCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  auto* navigation_request = NavigationRequest::From(&GetNavigationHandle());
  auto* render_frame_host =
      navigation_request->frame_tree_node()->current_frame_host();

  blink::mojom::PageSwapEventParamsPtr page_swap_event_params =
      navigation_request->WillDispatchPageSwap();
  CHECK(page_swap_event_params);

  CHECK(render_frame_host->IsRenderFrameLive());
  resume_processing_callback_ = std::move(resume);

  // Request a snapshot. This includes running any associaged script in the
  // renderer process.
  render_frame_host->GetAssociatedLocalFrame()
      ->SnapshotDocumentForViewTransition(
          std::move(page_swap_event_params),
          base::BindOnce(&ViewTransitionCommitDeferringCondition::OnSnapshotAck,
                         weak_factory_.GetWeakPtr(),
                         navigation_request->GetWeakPtr()));
  // Also post a timeout task to resume even if the renderer has not acked.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ViewTransitionCommitDeferringCondition::OnSnapshotTimeout,
                     weak_factory_.GetWeakPtr()),
      GetSnapshotCallbackTimeout());
  return Result::kDefer;
}

void ViewTransitionCommitDeferringCondition::OnSnapshotAck(
    base::WeakPtr<NavigationRequest> navigation_request,
    const blink::ViewTransitionState& view_transition_state) {
  if (!resume_processing_callback_) {
    return;
  }

  if (navigation_request && view_transition_state.HasElements()) {
    navigation_request->SetViewTransitionState(
        std::move(view_transition_state));
  }
  std::move(resume_processing_callback_).Run();
}

void ViewTransitionCommitDeferringCondition::OnSnapshotTimeout() {
  if (resume_processing_callback_) {
    std::move(resume_processing_callback_).Run();
  }
}

base::TimeDelta
ViewTransitionCommitDeferringCondition::GetSnapshotCallbackTimeout() const {
  // TODO(vmpstr): Figure out if we need to increase this in tests.
  return base::Seconds(4);
}

}  // namespace content
