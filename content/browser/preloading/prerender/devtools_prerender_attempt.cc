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
      attributes.prerendering_url, outcome, /*prerender_status=*/absl::nullopt);
}

void DevToolsPrerenderAttempt::SetFailureReason(
    const PrerenderAttributes& attributes,
    PrerenderFinalStatus status) {
  if (!attributes.initiator_devtools_navigation_token.has_value()) {
    return;
  }

  devtools_instrumentation::DidUpdatePrerenderStatus(
      attributes.initiator_frame_tree_node_id,
      attributes.initiator_devtools_navigation_token.value(),
      attributes.prerendering_url, PreloadingTriggeringOutcome::kFailure,
      status);
}

}  // namespace content
