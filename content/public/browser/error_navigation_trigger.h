// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ERROR_NAVIGATION_TRIGGER_H_
#define CONTENT_PUBLIC_BROWSER_ERROR_NAVIGATION_TRIGGER_H_

namespace content {
// A detailed reason on why errors in navigation happened. Mostly focusing on
// `net::ERR_ABORTED` failures.
// Note that despite the name, this might not be accompanied by `net::Errors` in
// the navigation, see also `NavigationHandle::GetErrorNavigationTrigger()`.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ErrorNavigationTrigger)
enum class ErrorNavigationTrigger {
  // An unknown trigger caused navigation abort.
  kUnknown = 0,

  // Caused by the cancelation from the navigation throttle.
  kNavigationThrottleCancel,

  // Caused by being blocked by the navigation throtle.
  kNavigationThrottleBlock,

  // Caused by the redirect not allowed due to the security policy.
  kRedirectNotAllowed,

  // Caused by the Credentialed subresource check blocked.
  kCredentialedSubresourceBlocked,

  // Caused by the embedder-initiated navigation of FencedFrame being blocked.
  kFencedFrameEmbedderInitiatedNavigation,

  // Caused by the permission policy of fenced frames being blocked.
  kFencedFramesPermissionPolicyBlocked,

  // Caused by the content decoder data pipe creation failing.
  kContentDecoderDataPipeCreationFailed,

  // The response should not be rendered (e.g. a download).
  kShouldNotRenderResponse,

  // The embedder overrides/blocks the URL load.
  kShouldOverrideUrlLoading,

  // The render initiated cross process navigation is not allowed, and blocked
  // the navigation.
  kRenderInitiatedCrossProcessNavigationNotAllowed,

  // The render initiated navigation could not request the URL.
  kRendererInitiatedCanNotRequestURL,

  // The response rendered fallback content, due to e.g. Http errors.
  kShouldRenderFallbackContent,

  // The navigation client was disconnected.
  kNavigationClientDisconnected,

  // The navigation failed due to silent errors caused by `net::ERR_ABORTED`.
  kFailedWithSilentErrorOnNetAborted,

  // The navigation failed due to silent errors caused by the error page being
  // suppressed due to custom handling of the error.
  kFailedWithSilentErrorOnIgnore,

  // The navigation failed due to silent errors caused by <webview> guests
  // suppressing the `net::ERR_BLOCKED_BY_CLIENT` error.
  kFailedWithSilentErrorOnBlockedByClient,

  // The navigation of kObject failed.
  kNavigationOfObjectFailed,

  // The navigation was same-document commit aborted by the renderer.
  kSameDocumentCommitAborted,

  // The navigation was aborted because the `RenderFrameHost` was deleted
  // before the navigation finished.
  kCommittedOnPendingDeletion,

  kMaxValue = kCommittedOnPendingDeletion,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:ErrorNavigationTrigger)
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ERROR_NAVIGATION_TRIGGER_H_
