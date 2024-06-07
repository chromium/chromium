// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_QUEUE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_QUEUE_H_

#include <memory>

#include "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_cancel_handler.h"

class OverlayRequest;
namespace web {
class WebState;
}

// A queue of OverlayRequests for a specific WebState.
class OverlayRequestQueue {
 public:
  OverlayRequestQueue(const OverlayRequestQueue&) = delete;
  OverlayRequestQueue& operator=(const OverlayRequestQueue&) = delete;

  virtual ~OverlayRequestQueue() = default;

  // Returns the request queue for `web_state` at `modality`.
  static OverlayRequestQueue* FromWebState(web::WebState* web_state,
                                           OverlayModality modality);

  // Create the OverlayRequestQueue for `web_state`.
  static void CreateForWebState(web::WebState* web_state);

  // Returns the number of requests in the queue.
  virtual size_t size() const = 0;

  // Returns the front request in the queue, or nullptr if the queue is empty.
  // The returned value should not be cached, as it may be destructed if the
  // queue is updated.
  virtual OverlayRequest* front_request() const = 0;

  // Returns the OverlayRequest at `index`.  `index` must be less than the
  // queue's size.  Also supports array-style accessors.
  virtual OverlayRequest* GetRequest(size_t index) const = 0;
  OverlayRequest* operator[](size_t index) { return GetRequest(index); }

  // Adds `request` to be displayed alongside the content area of queue's
  // corresponding WebState.  `cancel_handler` may be used to cancel the
  // request.  If `cancel_handler` is not provided, the request will be
  // cancelled by default for committed, document-changing navigations.
  virtual void AddRequest(std::unique_ptr<OverlayRequest> request,
                          std::unique_ptr<OverlayRequestCancelHandler>
                              cancel_handler = nullptr) = 0;

  // Inserts `request` into the queue at `index`.  `index` must be less than or
  // equal to the queue's size.  `cancel_handler` may be used to cancel the
  // request.  If `cancel_handler` is not provided, the request will be
  // cancelled by default for committed, document-changing navigations.
  // Inserting at index 0 will dismiss the currently visible overlay UI if it is
  // presented for that request.
  virtual void InsertRequest(size_t index,
                             std::unique_ptr<OverlayRequest> request,
                             std::unique_ptr<OverlayRequestCancelHandler>
                                 cancel_handler = nullptr) = 0;

  // Cancels the UI for all requests in the queue then empties the queue.
  virtual void CancelAllRequests() = 0;

 protected:
  OverlayRequestQueue() = default;

 private:
  friend class OverlayRequestCancelHandler;

  // Called by cancellation handlers to cancel `request`.
  virtual void CancelRequest(OverlayRequest* request) = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_QUEUE_H_
