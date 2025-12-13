// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_CONFIG_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_CONFIG_H_

#include <cstddef>

#include "base/auto_reset.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"

namespace content {

class ContentBrowserClient;

// Provides the parameters to configure the behaviour for back/forward visual
// transitions.
class CONTENT_EXPORT NavigationTransitionConfig {
 public:
  NavigationTransitionConfig() = delete;

  // Returns true if back forward visual transitions are supported for this
  // device.
  static bool SupportsBackForwardTransitions(
      base::PassKey<ContentBrowserClient>);

  // Computes the size of the screenshot cache.
  static size_t ComputeCacheSizeInBytes();

  // Provides the duration for a cache to be invisible before its evicted.
  static base::TimeDelta GetCleanupDelayForInvisibleCaches();

  // How long to wait after obtaining a copy of the contents in the GPU process
  // before sending it to the browser process.
  static base::TimeDelta ScreenshotSendResultDelay();

  // Sets the minimum required physical ram in Mb for the feature to be enabled.
  static base::AutoReset<int> SetMinRequiredPhysicalRamMbForTesting(int mb);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_CONFIG_H_
