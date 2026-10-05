// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DECLARATIVE_PERFORMANCE_OBSERVER_DECLARATIVE_PERFORMANCE_OBSERVER_COORDINATOR_H_
#define CONTENT_BROWSER_DECLARATIVE_PERFORMANCE_OBSERVER_DECLARATIVE_PERFORMANCE_OBSERVER_COORDINATOR_H_

#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

// Coordinates tab-scoped WebContentsObserver events and dispatches them to
// document-scoped DeclarativePerformanceObserver instances.
//
// Because DeclarativePerformanceObserver is a DocumentUserData, a single
// WebContents can contain multiple inactive or bfcached document observers at
// a time. Centralizing WebContentsObserver calls in this single tab-scoped
// coordinator avoids redundant event dispatching to background pages and routes
// updates specifically to the intended active frame.
class CONTENT_EXPORT DeclarativePerformanceObserverCoordinator
    : public WebContentsObserver,
      public WebContentsUserData<DeclarativePerformanceObserverCoordinator> {
 public:
  ~DeclarativePerformanceObserverCoordinator() override;

  DeclarativePerformanceObserverCoordinator(
      const DeclarativePerformanceObserverCoordinator&) = delete;
  DeclarativePerformanceObserverCoordinator& operator=(
      const DeclarativePerformanceObserverCoordinator&) = delete;

 private:
  friend class WebContentsUserData<DeclarativePerformanceObserverCoordinator>;
  explicit DeclarativePerformanceObserverCoordinator(WebContents* web_contents);

  // WebContentsObserver implementation.
  void OnVisibilityChanged(Visibility visibility) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameHostStateChanged(
      RenderFrameHost* render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_DECLARATIVE_PERFORMANCE_OBSERVER_DECLARATIVE_PERFORMANCE_OBSERVER_COORDINATOR_H_
