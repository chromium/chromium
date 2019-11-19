// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_QUEUE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_QUEUE_H_

#include "ios/chrome/browser/overlays/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/public/overlay_request_cancel_handler.h"

class OverlayRequest;
namespace web {
class WebState;
}

// A queue of OverlayRequests for a specific WebState.
class OverlayRequestQueue {
 public:
  virtual ~OverlayRequestQueue() = default;

  // Returns the request queue for |web_state| at |modality|.
  static OverlayRequestQueue* FromWebState(web::WebState* web_state,
                                           OverlayModality modality);

  // Adds |request| to be displayed alongside the content area of queue's
  // corresponding WebState.  The request may be cancelled by
  // |cancel_handler|.
  virtual void AddRequest(
      std::unique_ptr<OverlayRequest> request,
      std::unique_ptr<OverlayRequestCancelHandler> cancel_handler) = 0;

  // Adds |request| to the queue using a default cancellation handler that
  // cancels requests for committed, document-changing navigations.
  virtual void AddRequest(std::unique_ptr<OverlayRequest> request) = 0;

  // Returns the front request in the queue, or nullptr if the queue is empty.
  // The returned value should not be cached, as it may be destructed if the
  // queue is updated.
  virtual OverlayRequest* front_request() const = 0;

  // Cancels the UI for all requests in the queue then empties the queue.
  virtual void CancelAllRequests() = 0;

 protected:
  OverlayRequestQueue() = default;

 private:
  friend class OverlayRequestCancelHandler;
  DISALLOW_COPY_AND_ASSIGN(OverlayRequestQueue);

  // Called by cancellation handlers to cancel |request|.
  virtual void CancelRequest(OverlayRequest* request) = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_QUEUE_H_
