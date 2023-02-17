// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SUBFRAME_HISTORY_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_SUBFRAME_HISTORY_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// Defers subframe history navigations while waiting for a main-frame
// same-document history navigation. If a main-frame same-document navigation is
// cancelled by the web-exposed Navigation API, all associated subframe history
// navigations should also be cancelled.
class SubframeHistoryNavigationThrottle final : public NavigationThrottle {
 public:
  explicit SubframeHistoryNavigationThrottle(
      NavigationHandle* navigation_handle);
  SubframeHistoryNavigationThrottle(const SubframeHistoryNavigationThrottle&) =
      delete;
  SubframeHistoryNavigationThrottle& operator=(
      const SubframeHistoryNavigationThrottle&) = delete;
  ~SubframeHistoryNavigationThrottle() final;

  // NavigationThrottle method:
  ThrottleCheckResult WillStartRequest() final;
  ThrottleCheckResult WillCommitWithoutUrlLoader() final;
  const char* GetNameForLogging() final;
  void Resume() final;

  void Cancel();

  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* navigation_handle);

 private:
  base::WeakPtrFactory<SubframeHistoryNavigationThrottle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SUBFRAME_HISTORY_NAVIGATION_THROTTLE_H_
