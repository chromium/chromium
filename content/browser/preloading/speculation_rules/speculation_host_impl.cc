// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/speculation_rules/speculation_host_impl.h"

#include <functional>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/preloading_decider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

bool CandidatesAreValid(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  for (const auto& candidate : candidates) {
    // These non-http candidates should be filtered out in Blink and
    // SpeculationHostImpl should not see them. If SpeculationHostImpl receives
    // non-http candidates, it may mean the renderer process has a bug
    // or is compromised.
    if (!candidate->url.SchemeIsHTTPOrHTTPS()) {
      mojo::ReportBadMessage("SH_NON_HTTP");
      return false;
    }

    // Only "prerender" and "prerender-until-script" actions support
    // `target_browsing_context_name_hint`. Invalid Speculation Rules are
    // ignored and invalid candidates are not produced in Blink.
    if (candidate->action != blink::mojom::SpeculationAction::kPrerender &&
        candidate->action !=
            blink::mojom::SpeculationAction::kPrerenderUntilScript &&
        candidate->target_browsing_context_name_hint !=
            blink::mojom::SpeculationTargetHint::kNoHint) {
      mojo::ReportBadMessage("SH_TARGET_HINT_ON_PREFETCH");
      return false;
    }

    // Only "prefetch" action supports the requirement
    // "anonymous-client-ip-when-cross-origin". Invalid Speculation Rules are
    // ignored and invalid candidates are not produced in Blink.
    if (candidate->action != blink::mojom::SpeculationAction::kPrefetch &&
        candidate->requires_anonymous_client_ip_when_cross_origin) {
      mojo::ReportBadMessage(
          "SH_INVALID_REQUIRES_ANONYMOUS_CLIENT_IP_WHEN_CROSS_ORIGIN");
      return false;
    }

    // Speculation rules tags must contain at least one tag. When no tags are
    // specified in rules, this should contain std::nullopt that represents a
    // null tag.
    if (candidate->tags.empty()) {
      mojo::ReportBadMessage("SH_EMPTY_TAGS");
      return false;
    }
    // All speculation rules tags must be valid tokens and std::nullopt is valid
    // by definition.
    for (auto& tag : candidate->tags) {
      if (tag.has_value() &&
          !std::all_of(tag.value().begin(), tag.value().end(),
                       base::IsAsciiPrintable<char>)) {
        mojo::ReportBadMessage("SH_INVALID_TAG");
        return false;
      }
    }
  }
  return true;
}

// A renderer-selected candidate may only be enacted when the feature behind the
// selecting `heuristic` is enabled, mirroring the browser-side gating in
// AnchorElementInteractionHostImpl and PreloadingDecider. This stops a
// compromised renderer from enacting via a disabled heuristic.
bool HeuristicMayEnact(blink::mojom::SpeculationHeuristic heuristic) {
  switch (heuristic) {
    case blink::mojom::SpeculationHeuristic::kPointerDown:
    case blink::mojom::SpeculationHeuristic::kPointerHover:
      return true;
    case blink::mojom::SpeculationHeuristic::kViewportModerate:
      // The param mirrors
      // PreloadingDecider::OnModerateViewportHeuristicTriggered, which only
      // enacts candidates when it is set.
      return base::FeatureList::IsEnabled(
                 blink::features::kPreloadingModerateViewportHeuristics) &&
             blink::features::
                 kPreloadingModerateViewportHeuristicsEnactCandidates.Get();
    case blink::mojom::SpeculationHeuristic::kViewportEager:
      return base::FeatureList::IsEnabled(
          blink::features::kPreloadingEagerViewportHeuristics);
  }
}

}  // namespace

// static
void SpeculationHostImpl::Bind(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver) {
  CHECK(frame_host);
  // DocumentService will destroy this on pipe closure or frame destruction.
  new SpeculationHostImpl(*frame_host, std::move(receiver));
}

SpeculationHostImpl::SpeculationHostImpl(
    RenderFrameHost& frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver)
    : DocumentService(frame_host, std::move(receiver)) {}

SpeculationHostImpl::~SpeculationHostImpl() = default;

bool SpeculationHostImpl::ValidateFrameState() {
  // Window gets inactive randomly by user-interaction, which is not avoidable.
  if (!render_frame_host().IsActive()) {
    return false;
  }
  // Sending messages from sub frames won't happen unless it is compromised.
  if (render_frame_host().GetParent()) {
    mojo::ReportBadMessage(
        "SpeculationHost mojo message is sent from a subframe.");
    return false;
  }
  return true;
}

void SpeculationHostImpl::UpdateSpeculationCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates,
    bool enable_cross_origin_prerender_iframes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!CandidatesAreValid(candidates)) {
    return;
  }

  // Only handle messages from an active main frame.
  // TODO(crbug.com/489033320): Validate with ValidateFrameState().
  if (!render_frame_host().IsActive()) {
    return;
  }
  if (render_frame_host().GetParent()) {
    return;
  }

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->UpdateSpeculationCandidates(
      candidates, enable_cross_origin_prerender_iframes);
}

void SpeculationHostImpl::EnactCandidate(
    blink::mojom::SpeculationCandidatePtr candidate,
    blink::mojom::SpeculationHeuristic heuristic) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The renderer must only send EnactCandidate when renderer-side heuristics
  // are enabled; reject the message otherwise.
  if (!base::FeatureList::IsEnabled(
          blink::features::kSpeculationRulesRendererSideHeuristics)) {
    mojo::ReportBadMessage("SH_ENACT_CANDIDATE_FEATURE_DISABLED");
    return;
  }

  // The heuristic that selected the candidate must have its browser-side
  // feature (and enactment param) enabled.
  if (!HeuristicMayEnact(heuristic)) {
    mojo::ReportBadMessage("SH_ENACT_CANDIDATE_HEURISTIC_DISABLED");
    return;
  }

  // Validate the candidate the same way as UpdateSpeculationCandidates. A
  // compromised renderer must not be able to enact an invalid candidate.
  std::vector<blink::mojom::SpeculationCandidatePtr> singleton;
  singleton.push_back(std::move(candidate));
  if (!CandidatesAreValid(singleton)) {
    return;
  }

  // A pointer-hover selection must target a "moderate" or "eager" candidate
  // (matching PreloadingDecider::OnPointerHover). A compromised renderer must
  // not be able to enact a candidate with a different eagerness via the hover
  // heuristic.
  if (heuristic == blink::mojom::SpeculationHeuristic::kPointerHover) {
    const blink::mojom::SpeculationEagerness eagerness =
        singleton.front()->eagerness;
    if (eagerness != blink::mojom::SpeculationEagerness::kModerate &&
        eagerness != blink::mojom::SpeculationEagerness::kEager) {
      mojo::ReportBadMessage("SH_ENACT_CANDIDATE_INVALID_HOVER_EAGERNESS");
      return;
    }
  }

  // Only handle messages from an active main frame.
  // TODO(crbug.com/489033320): Validate with ValidateFrameState().
  if (!render_frame_host().IsActive()) {
    return;
  }
  if (render_frame_host().GetParent()) {
    return;
  }

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->EnactRendererSelectedCandidate(
      std::move(singleton.front()), heuristic);
}

void SpeculationHostImpl::OnLCPPredicted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!ValidateFrameState()) {
    return;
  }
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->OnLCPPredicted();
}



}  // namespace content
