// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STATUS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STATUS_H_

#include "content/public/browser/preloading.h"

namespace content {

// The various states that a prefetch can go through or terminate with. Used in
// UKM logging so don't remove or reorder values. Update
// |PrefetchProxyPrefetchStatus| in //tools/metrics/histograms/enums.xml
// whenever this is changed.
//
// If you change this, please follow the process
// https://docs.google.com/document/d/1PnrfowsZMt62PX1EvvTp2Nqs3ji1zrklrAEe1JYbkTk
// to ensure failure reasons are correctly shown in the DevTools
// frontend.
//
// If you change this, please follow the process in
// go/preloading-dashboard-updates to update the mapping reflected in dashboard,
// or if you are not a Googler, please file an FYI bug on https://crbug.new with
// component Internals>Preload.
// LINT.IfChange
enum class PrefetchStatus {
  // Deprecated. Replaced by `kPrefetchResponseUsed`.
  //
  // The interceptor used a prefetch.
  // kPrefetchUsedNoProbe = 0,

  // Deprecated. Probe success implies the response is used. Thus replaced
  // by `kPrefetchResponseUsed`.
  //
  // The interceptor used a prefetch after successfully probing the origin.
  // kPrefetchUsedProbeSuccess = 1,

  // The interceptor was not able to use an available prefetch because the
  // origin probe failed.
  kPrefetchNotUsedProbeFailed = 2,

  // The url was eligible to be prefetched, but the network request was never
  // made.
  kPrefetchNotStarted = 3,

  // Deprecated. No longer a reason for ineligibility.
  //
  // The url was not eligible to be prefetched because it is a Google-owned
  // domain.
  // kPrefetchIneligibleGoogleDomain = 4,

  // The url was not eligible to be prefetched because the user had cookies for
  // that origin.
  kPrefetchIneligibleUserHasCookies = 5,

  // The url was not eligible to be prefetched because there was a registered
  // service worker for that origin.
  kPrefetchIneligibleUserHasServiceWorker = 6,

  // The url was not eligible to be prefetched because its scheme was not
  // https://.
  kPrefetchIneligibleSchemeIsNotHttps = 7,

  // Deprecated. No longer a reason for ineligibility.
  //
  // The url was not eligible to be prefetched because its host was an IP
  // address.
  // kPrefetchIneligibleHostIsIPAddress = 8,

  // The url was not eligible to be prefetched because it uses a non-default
  // storage partition.
  kPrefetchIneligibleNonDefaultStoragePartition = 9,

  // The network request was cancelled before it finished. This happens when
  // there is a new navigation.
  kPrefetchNotFinishedInTime = 10,

  // The prefetch failed because of a net error.
  kPrefetchFailedNetError = 11,

  // The prefetch failed with a non-2XX HTTP response code.
  kPrefetchFailedNon2XX = 12,

  // The prefetch's Content-Type header was not supported.
  kPrefetchFailedMIMENotSupported = 13,

  // The prefetch finished successfully but was never used.
  kPrefetchSuccessful = 14,

  // Deprecated. No longer used.
  //
  // The navigation off of the Google SRP was to a url that was not on the SRP.
  // kNavigatedToLinkNotOnSRP = 15,

  // Deprecated. NSP no longer supported.
  //
  // Variants of the first three statuses with the additional context of a
  // successfully completed NoStatePrefetch.
  // kPrefetchUsedNoProbeWithNSP = 16,
  // kPrefetchUsedProbeSuccessWithNSP = 17,
  // kPrefetchNotUsedProbeFailedWithNSP = 18,

  // Deprecated. NSP no longer supported
  //
  // Variants of the first three statuses within the additional context of a
  // link that was eligible for NoStatePrefetch, but was not started because
  // the Prerender code denied the request.
  // kPrefetchUsedNoProbeNSPAttemptDenied = 19,
  // kPrefetchUsedProbeSuccessNSPAttemptDenied = 20,
  // kPrefetchNotUsedProbeFailedNSPAttemptDenied = 21,

  // Deprecated. NSP no longer supported.
  //
  // Variants of the first three statuses with in the additional context of a
  // link that was eligible for NoStatePrefetch that was never started.
  // kPrefetchUsedNoProbeNSPNotStarted = 22,
  // kPrefetchUsedProbeSuccessNSPNotStarted = 23,
  // kPrefetchNotUsedProbeFailedNSPNotStarted = 24,

  // Deprecated. Subresources no longer supported.
  //
  // A subresource which was not fetched because it was throttled by an
  // experimental control for the max number of subresources per prerender.
  // kSubresourceThrottled = 25,

  // Deprecated. No longer a reason for ineligibilty.
  //
  // The position of the link in the navigation prediction was not eligible to
  // be prefetch due to experiment controls.
  // kPrefetchPositionIneligible = 26,

  // A previous prefetch to the origin got a HTTP 503 response with an
  // Retry-After header that has no elapsed yet.
  kPrefetchIneligibleRetryAfter = 27,

  // A network error or intentional loadshed was previously encountered when
  // trying to setup a connection to the proxy and a prefetch should not be done
  // right now.
  kPrefetchIneligiblePrefetchProxyNotAvailable = 28,

  // The prefetch was not eligible, but was put on the network anyways and not
  // used to disguise that the user had some kind of previous relationship with
  // the origin.
  kPrefetchIsPrivacyDecoy = 29,

  // The prefetch was eligible, but too much time elapsed between the prefetch
  // and the interception.
  kPrefetchIsStale = 30,

  // Deprecated. NSP no longer supported
  // kPrefetchIsStaleWithNSP = 31,
  // kPrefetchIsStaleNSPAttemptDenied = 32,
  // kPrefetchIsStaleNSPNotStarted = 33,

  // The prefetch was not used because cookies were added to the URL after the
  // initial eligibility check.
  kPrefetchNotUsedCookiesChanged = 34,

  // Deprecated. Support for redirects added.
  //
  // The prefetch was redirected, but following redirects was disabled.
  // See crbug.com/1266876 for more details.
  // kPrefetchFailedRedirectsDisabled = 35,

  // The url was not eligible to be prefetched because its host was not unique
  // (e.g., a non publicly routable IP address or a hostname which is not
  // registry-controlled) but the prefetch was to be proxied.
  kPrefetchIneligibleHostIsNonUnique = 36,

  // The prefetch was not made because the user requested that the browser use
  // less data.
  kPrefetchIneligibleDataSaverEnabled = 37,

  // The URL is not eligible to be prefetched, because in the default network
  // context it is configured to use a proxy server.
  kPrefetchIneligibleExistingProxy = 38,

  // Prefetch not supported in Guest or Incognito mode.
  // OBSOLETE: kPrefetchIneligibleBrowserContextOffTheRecord = 39,

  // Whether this prefetch is heldback for counterfactual logging.
  kPrefetchHeldback = 40,
  kPrefetchAllowed = 41,

  // The response of the prefetch is used for the next navigation. This is the
  // final successful state.
  kPrefetchResponseUsed = 42,

  // The prefetch was redirected, but there was a problem with the redirect.
  kPrefetchFailedInvalidRedirect = 43,

  // The prefetch was redirected, but the redirect URL is not eligible for
  // prefetch.
  kPrefetchFailedIneligibleRedirect = 44,

  // Deprecated; prefetches are now queued until other prefetches are evicted
  // when the limit is reached.
  //
  // The prefetch was not made because prefetches exceeded the limit per
  // page.
  // kPrefetchFailedPerPageLimitExceeded = 45,

  // The prefetch needed to fetch a same-site cross-origin URL and required the
  // use of the prefetch proxy. These prefetches are blocked since the default
  // network context cannot be configured to use the prefetch proxy for a single
  // prefetch request.
  // TODO(crbug.com/40265797): Allow same-site cross-origin prefetches
  // that require the prefetch proxy to be made.
  kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy = 46,

  // The prefetch was not made because the `Battery Saver` setting was enabled.
  kPrefetchIneligibleBatterySaverEnabled = 47,

  // The prefetch was not made because preloading was disabled.
  kPrefetchIneligiblePreloadingDisabled = 48,

  // The prefetch was evicted to make room for a newer prefetch. This currently
  // only happens when |kPrefetchNewLimits| is enabled.
  // kPrefetchEvicted = 49, DEPRECATED
  kPrefetchEvictedAfterCandidateRemoved = 50,
  kPrefetchEvictedForNewerPrefetch = 51,

  // The max value of the PrefetchStatus. Update this when new enums are added.
  kMaxValue = kPrefetchEvictedForNewerPrefetch,
};
// LINT.ThenChange()

// Mapping from `PrefetchStatus` to `PreloadingFailureReason`.
static_assert(
    static_cast<int>(PrefetchStatus::kMaxValue) +
        static_cast<int>(
            PreloadingFailureReason::kPreloadingFailureReasonCommonEnd) <=
    static_cast<int>(
        PreloadingFailureReason::kPreloadingFailureReasonContentEnd));

inline PreloadingFailureReason ToPreloadingFailureReason(
    PrefetchStatus prefetch_container_metrics) {
  return static_cast<PreloadingFailureReason>(
      static_cast<int>(prefetch_container_metrics) +
      static_cast<int>(
          PreloadingFailureReason::kPreloadingFailureReasonCommonEnd));
}

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STATUS_H_
