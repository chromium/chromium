// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#import "base/memory/raw_ptr.h"

class OverlayRequest;
class OverlayRequestQueue;

// Handles the cancellation of OverlayRequests added to an OverlayRequestQueue.
class OverlayRequestCancelHandler {
 public:
  virtual ~OverlayRequestCancelHandler() = default;

 protected:
  // Constructor for a cancellation handler that cancels `request` from `queue`.
  OverlayRequestCancelHandler(OverlayRequest* request,
                              OverlayRequestQueue* queue);

  // Called by subclasses to cancel the associated request.
  void CancelRequest();

  // Accessors for the request and queue.
  OverlayRequest* request() const { return request_; }
  OverlayRequestQueue* queue() const { return queue_; }

 private:
  raw_ptr<OverlayRequest> request_ = nullptr;
  raw_ptr<OverlayRequestQueue> queue_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_CANCEL_HANDLER_H_
