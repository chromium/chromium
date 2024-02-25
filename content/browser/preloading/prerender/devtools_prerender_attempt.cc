// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/devtools_prerender_attempt.h"

#include "base/check_op.h"
#include "content/browser/devtools/devtools_instrumentation.h"

namespace content {

void DevToolsPrerenderAttempt::SetTriggeringOutcome(
    const PrerenderAttributes& attributes,
    PreloadingTriggeringOutcome outcome) {
  // PreloadingTriggeringOutcome::kFailure should be reported with
  // SetFailureReason to ensure having PrerenderFinalStatus.
  CHECK_NE(outcome, PreloadingTriggeringOutcome::kFailure);

  if (!attributes.initiator_devtools_navigation_token.has_value()) {
    return;
  }

  devtools_instrumentation::DidUpdatePrerenderStatus(
      attributes.initiator_frame_tree_node_id,
      attributes.initiator_devtools_navigation_token.value(),
      attributes.prerendering_url, attributes.target_hint, outcome,
      /*prerender_status=*/std::nullopt,
      /*disallowed_mojo_interface=*/std::nullopt,
      /*mismatched_headers=*/nullptr);
}

void DevToolsPrerenderAttempt::SetFailureReason(
    const PrerenderAttributes& attributes,
    PrerenderFinalStatus prerender_status) {
  // Ensured by PrerenderCancellationReason.
  CHECK_NE(prerender_status, PrerenderFinalStatus::kMojoBinderPolicy);
  CHECK_NE(prerender_status,
           PrerenderFinalStatus::kActivationNavigationParameterMismatch);

  if (!attributes.initiator_devtools_navigation_token.has_value()) {
    return;
  }

  devtools_instrumentation::DidUpdatePrerenderStatus(
      attributes.initiator_frame_tree_node_id,
      attributes.initiator_devtools_navigation_token.value(),
      attributes.prerendering_url, attributes.target_hint,
      PreloadingTriggeringOutcome::kFailure, prerender_status,
      /*disallowed_mojo_interface=*/std::nullopt,
      /*mismatched_headers=*/nullptr);
}

void DevToolsPrerenderAttempt::SetFailureReason(
    const PrerenderAttributes& attributes,
    const PrerenderCancellationReason& reason) {
  PrerenderFinalStatus prerender_status = reason.final_status();
  std::optional<std::string> disallowed_mojo_interface;
  const std::vector<PrerenderMismatchedHeaders>* mismatched_headers = nullptr;

  // Ensured by PrerenderCancellationReason.
  switch (prerender_status) {
    case PrerenderFinalStatus::kMojoBinderPolicy:
      disallowed_mojo_interface = reason.DisallowedMojoInterface();
      CHECK(disallowed_mojo_interface.has_value());
      break;
    case PrerenderFinalStatus::kActivationNavigationParameterMismatch:
      mismatched_headers = reason.GetPrerenderMismatchedHeaders();
      break;
    default:
      break;
  }

  if (!attributes.initiator_devtools_navigation_token.has_value()) {
    return;
  }

  devtools_instrumentation::DidUpdatePrerenderStatus(
      attributes.initiator_frame_tree_node_id,
      attributes.initiator_devtools_navigation_token.value(),
      attributes.prerendering_url, attributes.target_hint,
      PreloadingTriggeringOutcome::kFailure, prerender_status,
      disallowed_mojo_interface, mismatched_headers);
}

}  // namespace content
