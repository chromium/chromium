// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_CONFIG_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_CONFIG_H_

#include <cstddef>

#include "base/time/time.h"

namespace content {

// Provides the parameters to configure the behaviour for back/forward visual
// transitions.
class NavigationTransitionConfig {
 public:
  NavigationTransitionConfig() = delete;

  // Returns true if back forward visual transitions are enabled.
  static bool AreBackForwardTransitionsEnabled();

  // Computes the size of the screenshot cache.
  static size_t ComputeCacheSizeInBytes();

  // Provides the duration for a cache to be invisible before its evicted.
  static base::TimeDelta GetCleanupDelayForInvisibleCaches();
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_CONFIG_H_
