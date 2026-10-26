// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/devtools_prerender_attempt.h"

#include "base/check.h"
#include "base/check_op.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "services/network/public/cpp/headers_matcher.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {

namespace {

void DidUpdatePrerenderStatus(
    const PrerenderAttributes& attributes,
    std::optional<blink::mojom::SpeculationAction> effective_action,
    PreloadingTriggeringOutcome outcome,
    FrameTreeNodeId ftn_id,
    std::optional<PrerenderFinalStatus> prerender_status = std::nullopt,
    std::optional<std::string> disallowed_mojo_interface = std::nullopt,
    const std::vector<network::MismatchedHttpRequestHeader>*
        mismatched_headers = nullptr) {
  if (!attributes.initiator_devtools_navigation_token.has_value()) {
    return;
  }

  GURL url = attributes.prerendering_url;
  base::UnguessableToken preload_pipeline_id =
      attributes.preload_pipeline_info->id();
  if (!ftn_id.is_null()) {
    FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(ftn_id);
    if (!ftn) {
      return;
    }
    if (outcome == PreloadingTriggeringOutcome::kSuccess) {
      url = ftn->current_url();
    }
    preload_pipeline_id = ftn->current_frame_host()->devtools_frame_token();
  }

  devtools_instrumentation::DidUpdatePrerenderStatus(
      attributes.initiator_frame_tree_node_id,
      attributes.initiator_devtools_navigation_token.value(),
      attributes.prerender_action_type, effective_action, url,
      attributes.form_submission, attributes.GetTargetHint(),
      preload_pipeline_id, outcome, prerender_status, disallowed_mojo_interface,
      mismatched_headers);
}

}  // namespace

DevToolsPrerenderAttempt::DevToolsPrerenderAttempt() = default;
DevToolsPrerenderAttempt::~DevToolsPrerenderAttempt() = default;

void DevToolsPrerenderAttempt::SetTriggeringOutcome(
    const PrerenderAttributes& attributes,
    PreloadingTriggeringOutcome outcome,
    FrameTreeNodeId ftn_id) {
  // PreloadingTriggeringOutcome::kFailure should be reported with
  // SetFailureReason to ensure having PrerenderFinalStatus.
  CHECK_NE(outcome, PreloadingTriggeringOutcome::kFailure);
  if (outcome == PreloadingTriggeringOutcome::kSuccess) {
    for (auto& [id, last_outcome] : attempts_) {
      if (last_outcome == PreloadingTriggeringOutcome::kFailure) {
        continue;
      }
      last_outcome = outcome;
      DidUpdatePrerenderStatus(attributes, effective_action_, outcome, id);
    }
    return;
  }

  auto [it, inserted] = attempts_.try_emplace(ftn_id, outcome);
  if (!inserted) {
    CHECK_NE(it->second, PreloadingTriggeringOutcome::kFailure);
    it->second = outcome;
  }

  DidUpdatePrerenderStatus(attributes, effective_action_, outcome, ftn_id);
}

void DevToolsPrerenderAttempt::SetFailureReason(
    const PrerenderAttributes& attributes,
    PrerenderFinalStatus prerender_status,
    FrameTreeNodeId ftn_id) {
  // Ensured by PrerenderCancellationReason.
  CHECK_NE(prerender_status, PrerenderFinalStatus::kMojoBinderPolicy);
  CHECK_NE(prerender_status,
           PrerenderFinalStatus::kActivationNavigationParameterMismatch);
  attempts_[ftn_id] = PreloadingTriggeringOutcome::kFailure;

  DidUpdatePrerenderStatus(attributes, effective_action_,
                           PreloadingTriggeringOutcome::kFailure, ftn_id,
                           prerender_status);
}

void DevToolsPrerenderAttempt::SetFailureReason(
    const PrerenderAttributes& attributes,
    const PrerenderCancellationReason& reason,
    FrameTreeNodeId ftn_id) {
  PrerenderFinalStatus prerender_status = reason.final_status();
  std::optional<std::string> disallowed_mojo_interface;
  const std::vector<network::MismatchedHttpRequestHeader>* mismatched_headers =
      nullptr;

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
  attempts_[ftn_id] = PreloadingTriggeringOutcome::kFailure;

  DidUpdatePrerenderStatus(
      attributes, effective_action_, PreloadingTriggeringOutcome::kFailure,
      ftn_id, prerender_status, disallowed_mojo_interface, mismatched_headers);
}

void DevToolsPrerenderAttempt::SetEffectiveAction(
    const PrerenderAttributes& attributes,
    blink::mojom::SpeculationAction effective_action) {
  CHECK_EQ(attributes.prerender_action_type,
           blink::mojom::SpeculationAction::kPrerenderUntilScript);
  CHECK_EQ(effective_action, blink::mojom::SpeculationAction::kPrerender);
  effective_action_ = effective_action;
  auto it = attempts_.find(FrameTreeNodeId());
  if (it != attempts_.end()) {
    CHECK_NE(it->second, PreloadingTriggeringOutcome::kFailure);
    SetTriggeringOutcome(attributes, it->second);
  }
}

}  // namespace content
