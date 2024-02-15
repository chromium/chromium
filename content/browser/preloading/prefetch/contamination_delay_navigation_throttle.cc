// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/contamination_delay_navigation_throttle.h"

#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

ContaminationDelayNavigationThrottle::~ContaminationDelayNavigationThrottle() =
    default;

NavigationThrottle::ThrottleCheckResult
ContaminationDelayNavigationThrottle::WillProcessResponse() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  const network::mojom::URLResponseHead* response =
      navigation_request->response();

  if (response && response->is_prefetch_with_cross_site_contamination) {
    CHECK(navigation_request->IsInMainFrame())
        << "subframes should not use prefetches which may span network "
           "partitions";
    // This delay is approximately the amount of the the request would take if
    // we were sending a fresh request over a warm connection.
    base::TimeDelta delay = response->load_timing.receive_headers_end -
                            response->load_timing.send_start;
    timer_.Start(FROM_HERE, delay,
                 base::BindOnce(&ContaminationDelayNavigationThrottle::Resume,
                                base::Unretained(this)));
    return DEFER;
  }

  return PROCEED;
}

const char* ContaminationDelayNavigationThrottle::GetNameForLogging() {
  return "ContaminationDelayNavigationThrottle";
}

}  // namespace content
