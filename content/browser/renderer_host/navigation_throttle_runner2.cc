// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_throttle_runner2.h"

namespace content {

NavigationThrottleRunner2::NavigationThrottleRunner2(
    NavigationThrottleRegistryBase* registry,
    int64_t navigation_id,
    bool is_primary_main_frame)
    : registry_(*registry),
      navigation_id_(navigation_id),
      is_primary_main_frame_(is_primary_main_frame) {}

NavigationThrottleRunner2::~NavigationThrottleRunner2() = default;

void NavigationThrottleRunner2::ProcessNavigationEvent(
    NavigationThrottleEvent event) {
  // TODO(https://crbug.com/422003056): Implement this.
  registry_->OnEventProcessed(event, NavigationThrottle::PROCEED);
}

void NavigationThrottleRunner2::ResumeProcessingNavigationEvent(
    NavigationThrottle* resuming_throttle) {
  // TODO(https://crbug.com/422003056): Implement this.
  NOTREACHED();
}

}  // namespace content
