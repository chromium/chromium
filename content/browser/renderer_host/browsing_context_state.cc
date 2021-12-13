// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browsing_context_state.h"

namespace features {
const base::Feature kNewBrowsingContextStateOnBrowsingContextGroupSwap{
    "NewBrowsingContextStateOnBrowsingContextGroupSwap",
    base::FEATURE_DISABLED_BY_DEFAULT};

BrowsingContextStateImplementationType GetBrowsingContextMode() {
  if (base::FeatureList::IsEnabled(
          kNewBrowsingContextStateOnBrowsingContextGroupSwap)) {
    return BrowsingContextStateImplementationType::
        kSwapForCrossBrowsingInstanceNavigations;
  }

  return BrowsingContextStateImplementationType::
      kLegacyOneToOneWithFrameTreeNode;
}
}  // namespace features

namespace content {

BrowsingContextState::BrowsingContextState(
    blink::mojom::FrameReplicationStatePtr replication_state)
    : replication_state_(std::move(replication_state)) {}

BrowsingContextState::~BrowsingContextState() = default;

void BrowsingContextState::UpdateFramePolicy(
    const blink::FramePolicy& new_frame_policy,
    bool did_change_flags,
    bool did_change_container_policy,
    bool did_change_required_document_policy) {
  if (did_change_flags) {
    replication_state_->frame_policy.sandbox_flags =
        new_frame_policy.sandbox_flags;
  }
  if (did_change_container_policy) {
    replication_state_->frame_policy.container_policy =
        new_frame_policy.container_policy;
  }
  if (did_change_required_document_policy) {
    replication_state_->frame_policy.required_document_policy =
        new_frame_policy.required_document_policy;
  }
}
}  // namespace content