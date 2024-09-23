// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_DISCARD_REASON_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_DISCARD_REASON_H_

namespace content {
// Used to annotate the reason for calling into methods that delete speculative
// RenderFrameHosts or ongoing NavigationRequests. Purely informational for now,
// but in the future, intended to serve as a signal for how a caller of the
// functions should handle cases when the speculative RenderFrameHost cannot be
// promptly discarded. See https://crbug.com/1220337 for more info.
enum class NavigationDiscardReason {
  // A new navigation will start and replace a pre-existing navigation. This
  // resets any NavigationRequest and speculative RenderFrameHost on the
  // targeted FrameTreeNode. We use different enum values depending on the
  // type of the navigation.
  kNewReloadNavigation,
  kNewHistoryNavigation,
  kNewOtherNavigationBrowserInitiated,
  kNewOtherNavigationRendererInitiated,
  // The FrameTreeNode the navigation targets is being removed, e.g. user closed
  // the tab or script removed the frame owner element from its container
  // document.
  kWillRemoveFrame,
  // Ongoing navigations have been explicitly cancelled by external request e.g.
  // the user clicked the stop button, beforeunload cancellation, or a
  // renderer-initiated navigation's NavigationClient getting disconnected.
  kExplicitCancellation,
  // The navigation was stopped by navigation-internal checks, e.g.
  // NavigationThrottle.
  kInternalCancellation,
  // In certain cases when a navigation commits in a FrameTreeNode, other
  // navigation attempts targeting the same FrameTreeNode are cancelled.
  kCommittedNavigation,
  // The render process is gone, typically due to a crash.
  kRenderProcessGone,
  // The RenderFrameHost containing the NavigationRequest is destructed.
  // This is only used by the RenderFrameHost destructor, and typically other
  // navigation cancellations will cancel the navigations on the RFH separately
  // with a more specific reason before destructing the RenderFrameHost.
  kRenderFrameHostDestruction,
  // The NavigationRequest was created but it was never started, e.g. if it's
  // created for a history navigation that turns out to have a BFCache entry.
  kNeverStarted,
  // The NavigationRequest received a DidCommit response from the renderer, but
  // the response failed security checks (e.g. claiming an incompatible origin),
  // so the renderer got killed and the commit didn't go through.
  kFailedSecurityCheck,
  // Cancelled by a new navigation with identical conditions to the discarded
  // navigation.
  kNewDuplicateNavigation,
};

} // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_DISCARD_REASON_H_
