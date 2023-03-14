// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STATUS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STATUS_H_

namespace content {

// The various states that a prefetch can go through or terminate with. Used in
// UKM logging so don't remove or reorder values. Update
// |PrefetchProxyPrefetchStatus| in //tools/metrics/histograms/enums.xml
// whenever this is changed.
enum class PrefetchStatus {
  // The interceptor used a prefetch.
  kPrefetchUsedNoProbe = 0,

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

  // The url was not eligible to be prefetched because it is a Google-owned
  // domain.
  kPrefetchNotEligibleGoogleDomain = 4,

  // The url was not eligible to be prefetched because the user had cookies for
  // that origin.
  kPrefetchNotEligibleUserHasCookies = 5,

  // The url was not eligible to be prefetched because there was a registered
  // service worker for that origin.
  kPrefetchNotEligibleUserHasServiceWorker = 6,

  // The url was not eligible to be prefetched because its scheme was not
  // https://.
  kPrefetchNotEligibleSchemeIsNotHttps = 7,

  // The url was not eligible to be prefetched because its host was an IP
  // address. [DEPRECATED]
  // kPrefetchNotEligibleHostIsIPAddress = 8,

  // The url was not eligible to be prefetched because it uses a non-default
  // storage partition.
  kPrefetchNotEligibleNonDefaultStoragePartition = 9,

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

  // The navigation off of the Google SRP was to a url that was not on the SRP.
  kNavigatedToLinkNotOnSRP = 15,

  // Variants of the first three statuses with the additional context of a
  // successfully completed NoStatePrefetch.
  kPrefetchUsedNoProbeWithNSP = 16,
  kPrefetchUsedProbeSuccessWithNSP = 17,
  kPrefetchNotUsedProbeFailedWithNSP = 18,

  // Variants of the first three statuses within the additional context of a
  // link that was eligible for NoStatePrefetch, but was not started because
  // the Prerender code denied the request.
  kPrefetchUsedNoProbeNSPAttemptDenied = 19,
  kPrefetchUsedProbeSuccessNSPAttemptDenied = 20,
  kPrefetchNotUsedProbeFailedNSPAttemptDenied = 21,

  // Variants of the first three statuses with in the additional context of a
  // link that was eligible for NoStatePrefetch that was never started.
  kPrefetchUsedNoProbeNSPNotStarted = 22,
  kPrefetchUsedProbeSuccessNSPNotStarted = 23,
  kPrefetchNotUsedProbeFailedNSPNotStarted = 24,

  // A subresource which was not fetched because it was throttled by an
  // experimental control for the max number of subresources per prerender.
  kSubresourceThrottled = 25,

  // The position of the link in the navigation prediction was not eligible to
  // be prefetch due to experiment controls.
  kPrefetchPositionIneligible = 26,

  // A previous prefetch to the origin got a HTTP 503 response with an
  // Retry-After header that has no elapsed yet.
  kPrefetchIneligibleRetryAfter = 27,

  // A network error or intentional loadshed was previously encountered when
  // trying to setup a connection to the proxy and a prefetch should not be done
  // right now.
  kPrefetchProxyNotAvailable = 28,

  // The prefetch was not eligible, but was put on the network anyways and not
  // used to disguise that the user had some kind of previous relationship with
  // the origin.
  kPrefetchIsPrivacyDecoy = 29,

  // The prefetch was eligible, but too much time elapsed between the prefetch
  // and the interception.
  kPrefetchIsStale = 30,
  kPrefetchIsStaleWithNSP = 31,
  kPrefetchIsStaleNSPAttemptDenied = 32,
  kPrefetchIsStaleNSPNotStarted = 33,

  // The prefetch was not used because cookies were added to the URL after the
  // initial eligibility check.
  kPrefetchNotUsedCookiesChanged = 34,

  // The prefetch was redirected, but following redirects was disabled.
  // See crbug.com/1266876 for more details.
  kPrefetchFailedRedirectsDisabled_DEPRECATED = 35,

  // The url was not eligible to be prefetched because its host was not unique
  // (e.g., a non publicly routable IP address or a hostname which is not
  // registry-controlled) but the prefetch was to be proxied.
  kPrefetchNotEligibleHostIsNonUnique = 36,

  // The prefetch was not made because the user requested that the browser use
  // less data.
  kPrefetchNotEligibleDataSaverEnabled = 37,

  // The URL is not eligible to be prefetched, because in the default network
  // context it is configured to use a proxy server.
  kPrefetchNotEligibleExistingProxy = 38,

  // Prefetch not supported in Guest or Incognito mode.
  kPrefetchNotEligibleBrowserContextOffTheRecord = 39,

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

  // The max value of the PrefetchStatus. Update this when new enums are added.
  kMaxValue = kPrefetchFailedIneligibleRedirect,
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STATUS_H_
