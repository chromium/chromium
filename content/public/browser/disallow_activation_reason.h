// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DISALLOW_ACTIVATION_REASON_H_
#define CONTENT_PUBLIC_BROWSER_DISALLOW_ACTIVATION_REASON_H_

#include "content/public/browser/render_frame_host.h"

namespace content {

// Reasons to disallow activation of an inactive RenderFrameHost. Disallows
// a page to be restored from bfache or used for prerendering activation,
// leading to eviction/cancellation.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. kMaxValue is not defined because this
// enum is not logged directly as an enum because external embedders can supply
// their own values.
//
// This enum's values should not be used by embedders. Embedders should use
// values equal to or greater than kMinEmbedderDisallowActivationReason.
enum DisallowActivationReasonId : uint64_t {
  kForTesting = 0,
  kLoadPostCommitErrorPage = 1,
  kBeginNavigation = 2,
  kCapturePaintPreview = 3,
  kOpenURL = 4,
  kAXEvent = 5,
  kAXLocationChange = 6,
  kAXUpdateTree = 7,
  kAXHitTest = 8,
  kAXHitTestCallback = 9,
  kAXPerformAction = 10,
  kAXSetFocus = 11,
  kAXGetNativeView = 12,
  kAXGetNativeViewForWindow = 13,
  // kAXWebContents = 14 is no longer blocking.
  kCertificateErrors = 15,
  kCreateChildFrame = 16,
  kCommitSameDocumentNavigation = 17,
  kFullScreenStateChanged = 18,
  kSetNeedsOcclusionTracking = 19,
  kDispatchLoad = 20,
  kForwardResourceTimingToParent = 21,
  kCapturePaintPreviewProxy = 22,
  kShowContextMenu = 23,
  kDetermineActionForHistoryNavigation = 24,
  kNavigatingInInactiveFrame = 25,
  kJsInjectionPostMessage = 26,
  kRequestPermission = 27,
  kPermissionRequestSource = 28,
  kPermissionAddRequest = 29,
  kContentsPreferredSizeChanged = 30,
  kBeginDownload = 31,
  kBug1234857 = 32,
  kFileSystemAccessPermissionRequest = 33,
  kCreateFencedFrame = 34,
  kIndexedDBEvent = 35,
  kIndexedDBTransactionIsAcquiringLocks = 36,
  // kIndexedDBTransactionIsBlockingOthers = 37 is deprecated.
  kSafeBrowsingUnsafeSubresource = 38,
  kFileSystemAccessLockingContention = 39,
  kIndexedDBTransactionIsStartingWhileBlockingOthers = 40,
  kIndexedDBTransactionIsOngoingAndBlockingOthers = 41,
  // New entries go above here. New entries should be added to
  // tools/metrics/histograms/enums.xml .
  kMinEmbedderDisallowActivationReason = 2 << 16,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DISALLOW_ACTIVATION_REASON_H_
