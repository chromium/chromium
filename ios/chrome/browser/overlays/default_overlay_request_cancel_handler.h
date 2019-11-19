// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_DEFAULT_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_DEFAULT_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#include "base/scoped_observer.h"
#import "ios/chrome/browser/overlays/public/overlay_request_cancel_handler.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

// A default implementation of OverlayRequestCancelHandler.  Cancels the request
// for committed, document-changing navigations.
class DefaultOverlayRequestCancelHandler : public OverlayRequestCancelHandler {
 public:
  DefaultOverlayRequestCancelHandler(OverlayRequest* request,
                                     OverlayRequestQueue* queue,
                                     web::WebState* web_state);
  ~DefaultOverlayRequestCancelHandler() override;

 private:
  // Cancels the request for navigation events.
  void Cancel();

  // Helper object that intercepts navigation events to trigger cancellation.
  class NavigationHelper : web::WebStateObserver {
   public:
    NavigationHelper(DefaultOverlayRequestCancelHandler* cancel_handler,
                     web::WebState* web_state);
    ~NavigationHelper() override;

    // web::WebStateObserver:
    void DidFinishNavigation(
        web::WebState* web_state,
        web::NavigationContext* navigation_context) override;
    void RenderProcessGone(web::WebState* web_state) override;
    void WebStateDestroyed(web::WebState* web_state) override;

   private:
    DefaultOverlayRequestCancelHandler* cancel_handler_ = nullptr;
    ScopedObserver<web::WebState, web::WebStateObserver> scoped_observer_;
  };

  NavigationHelper navigation_helper_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_DEFAULT_OVERLAY_REQUEST_CANCEL_HANDLER_H_
