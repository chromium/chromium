// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_METRICS_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_METRICS_H_

#include <optional>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

class NavigationHandle;
class RenderFrameHost;

// Holds metrics related to the prefetches requested by a page load.
struct CONTENT_EXPORT PrefetchReferringPageMetrics {
  static std::optional<PrefetchReferringPageMetrics> GetForCurrentDocument(
      RenderFrameHost* rfh);

  PrefetchReferringPageMetrics() = default;
  ~PrefetchReferringPageMetrics() = default;

  PrefetchReferringPageMetrics(const PrefetchReferringPageMetrics&) = default;
  PrefetchReferringPageMetrics& operator=(const PrefetchReferringPageMetrics&) =
      default;

  // The number of prefetches that were attempted by this page.
  int prefetch_attempted_count = 0;

  // The number of prefetches that were determined to be eligible.
  int prefetch_eligible_count = 0;

  // The number of prefetches that completed successfully.
  int prefetch_successful_count = 0;
};

// Holds metrics related to page loads that may benefit from prefetches
// triggered by speculation rules.
struct CONTENT_EXPORT PrefetchServingPageMetrics {
  static std::optional<PrefetchServingPageMetrics> GetForNavigationHandle(
      NavigationHandle& navigation_handle);

  PrefetchServingPageMetrics() = default;
  ~PrefetchServingPageMetrics() = default;

  PrefetchServingPageMetrics(const PrefetchServingPageMetrics&) = default;
  PrefetchServingPageMetrics& operator=(const PrefetchServingPageMetrics&) =
      default;

  // The |PrefetchStatus| of the prefetch used when serving this page.
  std::optional<int> prefetch_status;

  // Whether or not the prefetch required the use of a private prefetch proxy.
  bool required_private_prefetch_proxy = false;

  // Whether or not this page load is in the same WebContents as the page the
  // requested the page load.
  bool same_tab_as_prefetching_tab = false;

  // The time between the prefetch request was sent and the time the response
  // headers were received.
  std::optional<base::TimeDelta> prefetch_header_latency;

  // The amount of time it took to get the result of the probe. Only set if
  // the probe result was checked.
  std::optional<base::TimeDelta> probe_latency;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_METRICS_H_
