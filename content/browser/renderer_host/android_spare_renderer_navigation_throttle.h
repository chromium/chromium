// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ANDROID_SPARE_RENDERER_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_ANDROID_SPARE_RENDERER_NAVIGATION_THROTTLE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {

// AndroidSpareRendererNavigationThrottle applies throttles for navigation that
// uses the spare renderer on Android. It will block the navigation request
// from being sent until the complete callback is triggered for
// RenderProcessHostImpl::GraduateSpareToNormalRendererPriority.
// The throttle is added to ensure the process priority of the renderer process
// so that the renderer is not killed by LMKD because of the asynchronous
// priority update on Android. Since navigation is robust to the renderer
// process kill before receiving the network response, the killed renderer
// process can be relaunched. Using spare renderer and blocking the navigation
// should be faster than launching a new renderer without spare renderer.
class CONTENT_EXPORT AndroidSpareRendererNavigationThrottle
    : public NavigationThrottle,
      public RenderProcessHostObserver {
 public:
  static void CreateAndAdd(NavigationThrottleRegistry& registry);

  explicit AndroidSpareRendererNavigationThrottle(
      NavigationThrottleRegistry& registry);
  ~AndroidSpareRendererNavigationThrottle() override;

  // NavigationThrottle implementation:
  // TODO(crbug.com/447645527): Support non-URLLoader navigations as well
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

  // RenderProcessHostObserver implementation:
  void SpareRendererPriorityGraduated(RenderProcessHost* host,
                                      bool is_alive) override;
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

 protected:
  virtual RenderProcessHost* GetSpeculativeRenderProcessHost();

 private:
  void MaybeResume(bool is_alive);

  // The speculative render process host the throttle is observing.
  // The RPH is saved for removing the RenderProcessHostObserver.
  // If the render_process_host_ gets destructed first, it will be
  // reset in RenderProcessHostDestroyed first so it is safe to keep a raw_ptr
  // here.
  raw_ptr<RenderProcessHost> render_process_host_;

  std::unique_ptr<base::ElapsedTimer> defer_timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ANDROID_SPARE_RENDERER_NAVIGATION_THROTTLE_H_
