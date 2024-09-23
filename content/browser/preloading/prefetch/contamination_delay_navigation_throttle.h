// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_CONTAMINATION_DELAY_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_CONTAMINATION_DELAY_NAVIGATION_THROTTLE_H_

#include "base/timer/timer.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// Delays the commit of navigations where cross-site state may have been used to
// determine whether the prefetch can proceed, and therefore the timing of
// commit would otherwise reveal information about those checks.
class ContaminationDelayNavigationThrottle : public NavigationThrottle {
 public:
  using NavigationThrottle::NavigationThrottle;
  ~ContaminationDelayNavigationThrottle() override;

  // NavigationThrottle:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  base::OneShotTimer timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_CONTAMINATION_DELAY_NAVIGATION_THROTTLE_H_
