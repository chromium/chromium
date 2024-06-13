// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_transition_data.h"

#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"

namespace content {

void NavigationTransitionData::SetSameDocumentNavigationEntryScreenshotToken(
    const std::optional<blink::SameDocNavigationScreenshotDestinationToken>&
        token) {
  viz::HostFrameSinkManager* manager = GetHostFrameSinkManager();
  CHECK(manager);
  if (same_document_navigation_entry_screenshot_token_.has_value()) {
    manager->InvalidateCopyOutputReadyCallback(
        same_document_navigation_entry_screenshot_token_.value());
  }
  same_document_navigation_entry_screenshot_token_ = token;
}

}  // namespace content
