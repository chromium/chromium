// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_DEFAULT_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_DEFAULT_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_cancel_handler.h"
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
  class NavigationHelper : public web::WebStateObserver {
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
    raw_ptr<DefaultOverlayRequestCancelHandler> cancel_handler_ = nullptr;
    base::ScopedObservation<web::WebState, web::WebStateObserver>
        scoped_observation_{this};
  };

  NavigationHelper navigation_helper_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_DEFAULT_OVERLAY_REQUEST_CANCEL_HANDLER_H_
