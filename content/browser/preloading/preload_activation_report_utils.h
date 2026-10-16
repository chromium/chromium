// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_UTILS_H_

#include "content/common/content_export.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {

enum class PreloadActivationBeaconFeatureState {
  kEnabled,
  kDisabledByFeatureFlag,
  kNoOriginTrialToken,
};

// Reason why the activation beacon is handled or not handled when receiving the
// activation beacon header in `NavigationRequest::OnResponseStarted`.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PreloadActivationBeaconStatus)
enum class PreloadActivationBeaconStatus {
  // Disabled by local feature flag (explicitly overridden to false).
  kDisabledByFeatureFlag = 0,
  // Disabled because the feature is not enabled by default/Finch and no valid
  // Origin Trial token is present in the response headers.
  kNoOriginTrialToken = 1,
  // Activation beacon is handled by prefetched navigation.
  kHandledByPrefetch = 2,
  // Activation beacon is handled by prerender activation.
  kHandledByPrerender = 3,
  // While the feature is enabled and the header is present, the navigation
  // itself is not in a prefetch/preload context.
  kNotInPreloadContext = 4,
  // The activation beacon endpoint URL is cross-origin with the request URL.
  kCrossOrigin = 5,
  kMaxValue = kCrossOrigin,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PreloadActivationBeaconStatus)

// The type of navigation when an activation beacon header is received on a
// navigation that is not in a prefetch/preload context.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PreloadActivationBeaconNonPreloadNavigationType)
enum class PreloadActivationBeaconNonPreloadNavigationType {
  kTabRestore = 0,
  kRefresh = 1,
  kBackForward = 2,
  kCommonNavigation = 3,
  kMaxValue = kCommonNavigation,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PreloadActivationBeaconNonPreloadNavigationType)

// Returns the feature state for PrefetchActivationBeacon, accounting for local
// feature flag overrides and the Origin Trial token in `response_headers`.
//
// Since this feature is checked in the browser process, and the origin trial
// token is checked against response headers, we manually validate the origin
// trial token from the headers here if the base::Feature is disabled by
// default. If the feature is explicitly kill-switched via Finch/command-line
// (overridden to disabled), this will return `kDisabledByFeatureFlag`.
CONTENT_EXPORT PreloadActivationBeaconFeatureState
GetPrefetchActivationBeaconFeatureState(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers);

// Returns the feature state for PrerenderActivationBeacon, accounting for local
// feature flag overrides and the Origin Trial token in `response_headers`.
//
// Similar to GetPrefetchActivationBeaconFeatureState, we manually validate the
// origin trial token from the headers here if the base::Feature is disabled by
// default, and respect any explicit override (kill-switch).
CONTENT_EXPORT PreloadActivationBeaconFeatureState
GetPrerenderActivationBeaconFeatureState(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_UTILS_H_
