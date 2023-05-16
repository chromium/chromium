// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_STATUS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_STATUS_H_

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PrefetchStreamingURLLoaderStatus {
  // The streaming URL loader is in progress.
  kWaitingOnHead = 0,
  kHeadReceivedWaitingOnBody = 1,

  // The request redirected to a different target.
  kRedirected_DEPRECATED = 2,

  // Both the head and body of the response were received successfully.
  kSuccessfulNotServed = 3,
  kSuccessfulServedAfterCompletion = 4,
  kSuccessfulServedBeforeCompletion = 5,

  // Failure reasons based on the head of the response.
  kPrefetchWasDecoy = 6,
  kFailedInvalidHead = 7,
  kFailedInvalidHeaders = 8,
  kFailedNon2XX = 9,
  kFailedMIMENotSupported = 10,

  // Failure reasons where the head of the response was good, but an error
  // occurred while receiving the body of the response.
  kFailedNetError = 11,
  kFailedNetErrorButServed = 12,

  // Statuses related to redirects.
  kFollowRedirect = 13,
  kPauseRedirectForEligibilityCheck_DEPRECATED = 14,
  kFailedInvalidRedirect = 15,
  kStopSwitchInNetworkContextForRedirect = 16,

  // The streaming URL loader was previously stopped after a redirect required a
  // change in network context, and is served.
  kServedSwitchInNetworkContextForRedirect = 17,

  kMaxValue = kServedSwitchInNetworkContextForRedirect,
};

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_STATUS_H_
