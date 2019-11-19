// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#include <memory>

class OverlayRequest;
class OverlayRequestQueue;

// Handles the cancellation of OverlayRequests added to an OverlayRequestQueue.
class OverlayRequestCancelHandler {
 public:
  virtual ~OverlayRequestCancelHandler() = default;

 protected:
  // Constructor for a cancellation handler that cancels |request| from |queue|.
  OverlayRequestCancelHandler(OverlayRequest* request,
                              OverlayRequestQueue* queue);

  // Called by subclasses to cancel the associated request.
  void CancelRequest();

 private:
  OverlayRequest* request_ = nullptr;
  OverlayRequestQueue* queue_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_CANCEL_HANDLER_H_
