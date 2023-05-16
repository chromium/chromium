// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FINAL_STATUS_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FINAL_STATUS_H_

#include "content/public/browser/preloading.h"

#include "content/common/content_export.h"

namespace content {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// If you change this, please follow the process in
// go/preloading-dashboard-updates to update the mapping reflected in
// dashboard, or if you are not a Googler, please file an FYI bug on
// https://crbug.new with component Internals>Preload.
enum class PrerenderFinalStatus {
  kActivated = 0,
  kDestroyed = 1,
  kLowEndDevice = 2,
  // These have been broken down into SameSiteCrossOrigin and CrossSite for
  // better understanding of the metrics. kCrossOriginRedirect = 3,
  // kCrossOriginNavigation = 4,
  kInvalidSchemeRedirect = 5,
  kInvalidSchemeNavigation = 6,
  kInProgressNavigation = 7,
  // kNavigationRequestFailure = 8,  // No longer used.
  kNavigationRequestBlockedByCsp = 9,
  kMainFrameNavigation = 10,
  kMojoBinderPolicy = 11,
  // kPlugin = 12,  // No longer used.
  kRendererProcessCrashed = 13,
  kRendererProcessKilled = 14,
  kDownload = 15,
  kTriggerDestroyed = 16,
  kNavigationNotCommitted = 17,
  kNavigationBadHttpStatus = 18,
  kClientCertRequested = 19,
  kNavigationRequestNetworkError = 20,
  kMaxNumOfRunningPrerendersExceeded = 21,
  kCancelAllHostsForTesting = 22,
  kDidFailLoad = 23,
  kStop = 24,
  kSslCertificateError = 25,
  kLoginAuthRequested = 26,
  kUaChangeRequiresReload = 27,
  kBlockedByClient = 28,
  kAudioOutputDeviceRequested = 29,
  kMixedContent = 30,
  kTriggerBackgrounded = 31,
  // Break down into kEmbedderTriggeredAndSameOriginRedirected and
  // kEmbedderTriggeredAndCrossOriginRedirected for investigation.
  // kEmbedderTriggeredAndRedirected = 32,
  // Deprecate since same origin redirection is allowed considering that the
  // initial prerender origin is a safe site.
  // kEmbedderTriggeredAndSameOriginRedirected = 33,
  kEmbedderTriggeredAndCrossOriginRedirected = 34,
  // Deprecated. This has the same meaning as kTriggerDestroyed because the
  // metric's name includes trigger type.
  // kEmbedderTriggeredAndDestroyed = 35,
  kMemoryLimitExceeded = 36,
  kFailToGetMemoryUsage = 37,
  kDataSaverEnabled = 38,
  kHasEffectiveUrl = 39,
  kActivatedBeforeStarted = 40,
  kInactivePageRestriction = 41,
  kStartFailed = 42,
  kTimeoutBackgrounded = 43,

  // Enums for prerender initial navigation. For main frame navigation in
  // prerendered pages after prerender initial navigation, use enums suffixed
  // with InMainFrameNavigation (e.g., kCrossSiteRedirectInMainFrameNavigation).
  kCrossSiteRedirectInInitialNavigation = 44,
  kCrossSiteNavigationInInitialNavigation = 45,
  // Deprecated. Same-site cross-origin navigation in a prerendered page is
  // allowed in crbug.com/1239281.
  // kSameSiteCrossOriginRedirectInInitialNavigation = 46,
  // kSameSiteCrossOriginNavigationInInitialNavigation = 47,
  kSameSiteCrossOriginRedirectNotOptInInInitialNavigation = 48,
  kSameSiteCrossOriginNavigationNotOptInInInitialNavigation = 49,

  // The prediction is correct, and we are almost ready to activate this
  // PrerenderHost, but the activation navigation's parameters are different
  // from the initial prerendering navigation so Prerender fails to activate it.
  kActivationNavigationParameterMismatch = 50,
  kActivatedInBackground = 51,
  kEmbedderHostDisallowed = 52,
  // Called when encounter failures during synchronous activation.
  // TODO(https://crbug.com/1363550): Remove this reason if no sample is
  // recorded in stable, or look into the reason if there are.
  kActivationNavigationDestroyedBeforeSuccess = 53,
  // See comments on WebContents::kTabClosedWithoutUserGesture for the
  // difference between the two statuses below.
  kTabClosedByUserGesture = 54,
  kTabClosedWithoutUserGesture = 55,
  kPrimaryMainFrameRendererProcessCrashed = 56,
  kPrimaryMainFrameRendererProcessKilled = 57,
  kActivationFramePolicyNotCompatible = 58,
  kPreloadingDisabled = 59,
  kBatterySaverEnabled = 60,
  kActivatedDuringMainFrameNavigation = 61,
  kPreloadingUnsupportedByWebContents = 62,

  // Enums for main frame navigation in prerendered pages.
  kCrossSiteRedirectInMainFrameNavigation = 63,
  kCrossSiteNavigationInMainFrameNavigation = 64,
  kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation = 65,
  kSameSiteCrossOriginNavigationNotOptInInMainFrameNavigation = 66,

  kMemoryPressureOnTrigger = 67,
  kMemoryPressureAfterTriggered = 68,

  kMaxValue = kMemoryPressureAfterTriggered,
};

// Helper method to convert PrerenderFinalStatus to PreloadingFailureReason.
PreloadingFailureReason CONTENT_EXPORT
    ToPreloadingFailureReason(PrerenderFinalStatus);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FINAL_STATUS_H_
