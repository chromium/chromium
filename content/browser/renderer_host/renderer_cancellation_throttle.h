// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDERER_CANCELLATION_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDERER_CANCELLATION_THROTTLE_H_

#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// Renderer-initiated navigations can be canceled from the JS task it was
// initiated from, e.g. if the script runs window.stop() after initiating the
// navigation. See also https://github.com/whatwg/html/issues/3447 and
// https://crbug.com/763106 for more background.
//
// The renderer cancels navigation by triggering the disconnection of the
// NavigationClient interface used to start the navigation, eventually
// calling `NavigationRequest::OnRendererAbortedNavigation()`.
//
// Same-SiteInstanceGroup navigations used to use the same NavigationClient for
// starting and committing the navigation. This means even if a CommitNavigation
// IPC is in-flight at the time of navigation cancellation, the navigation can
// still get canceled. Also, since the same RenderFrame is reused, the
// CommitNavigation IPC also implicitly waits for the JS task that triggers the
// navigation to finish, as the commit can't be processed before then.
//
// However, with RenderDocument, the RenderFrame and NavigationClient won't be
// reused, which means navigation cancellations might only affect navigations
// that haven't entered READY_TO_COMMIT stage.
//
// RendererCancellationThrottle helps preserve the previous behavior by waiting
// for the JS task to finish, through deferring the navigation before it gets
// into the READY_TO_COMMIT stage, until the renderer that started the
// navigation sends the `NavigationCancellationWindowEnded` IPC that corresponds
// to the navigation, signifying that the JS task that initiated the navigation
// had ended and no renderer-initiated navigation cancellations can happen after
// that point.
class CONTENT_EXPORT RendererCancellationThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<RendererCancellationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* handle);

  // Sets the cancellation timeout. Resets the timeout to the default value if
  // `timeout` is zero.
  static void SetCancellationTimeoutForTesting(base::TimeDelta timeout);

  explicit RendererCancellationThrottle(NavigationHandle* navigation_handle);
  ~RendererCancellationThrottle() override;
  RendererCancellationThrottle() = delete;
  RendererCancellationThrottle(const RendererCancellationThrottle&) = delete;
  RendererCancellationThrottle& operator=(const RendererCancellationThrottle&) =
      delete;

  // The renderer had indicated that the navigation cancellation window had
  // ended, so the navigation can resume.
  void NavigationCancellationWindowEnded();

 private:
  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override;
  NavigationThrottle::ThrottleCheckResult WillCommitWithoutUrlLoader() override;
  const char* GetNameForLogging() override;

  NavigationThrottle::ThrottleCheckResult WaitForRendererCancellationIfNeeded();
  void OnTimeout();
  void RestartTimeout();

  base::OneShotTimer renderer_cancellation_timeout_timer_;

  base::WeakPtrFactory<RendererCancellationThrottle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDERER_CANCELLATION_THROTTLE_H_