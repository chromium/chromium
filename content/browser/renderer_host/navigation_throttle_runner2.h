// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_RUNNER2_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_RUNNER2_H_

#include <stdint.h>

#include "base/memory/raw_ref.h"
#include "base/memory/safety_checks.h"
#include "content/browser/renderer_host/navigation_throttle_registry_impl.h"
#include "content/common/content_export.h"

namespace content {

// This is a revamped implementation of the NavigationThrottleRunner that is
// used behind a feature flag. See https://crbug.com/422003056 for more details.
class CONTENT_EXPORT NavigationThrottleRunner2
    : public NavigationThrottleRunnerBase {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  // `registry` should outlive this instance.
  NavigationThrottleRunner2(NavigationThrottleRegistryBase* registry,
                            int64_t navigation_id,
                            bool is_primary_main_frame);

  NavigationThrottleRunner2(const NavigationThrottleRunner2&) = delete;
  NavigationThrottleRunner2& operator=(const NavigationThrottleRunner2&) =
      delete;

  ~NavigationThrottleRunner2() override;

 private:
  // Implements NavigationThrottleRunnerBase:
  void ProcessNavigationEvent(NavigationThrottleEvent event) override;
  void ResumeProcessingNavigationEvent(
      NavigationThrottle* resuming_throttle) override;

  const raw_ref<NavigationThrottleRegistryBase> registry_;

  // The unique id of the navigation which this throttle runner is associated
  // with.
  const int64_t navigation_id_;

  // Whether the navigation is in the primary main frame.
  bool is_primary_main_frame_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_RUNNER2_H_
