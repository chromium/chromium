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

BrowsingContextState::BrowsingContextState() = default;

BrowsingContextState::~BrowsingContextState() = default;
}  // namespace content