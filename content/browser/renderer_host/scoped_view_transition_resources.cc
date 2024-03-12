// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/scoped_view_transition_resources.h"

#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"

namespace content {

ScopedViewTransitionResources::ScopedViewTransitionResources(
    viz::NavigationId navigation_id)
    : navigation_id_(navigation_id) {}

ScopedViewTransitionResources::~ScopedViewTransitionResources() {
  GetHostFrameSinkManager()->ClearUnclaimedViewTransitionResources(
      navigation_id_);
}

}  // namespace content
