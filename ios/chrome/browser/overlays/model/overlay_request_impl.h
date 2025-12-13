// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_REQUEST_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_REQUEST_IMPL_H_

#import "base/memory/raw_ptr.h"
#include "ios/chrome/browser/overlays/model/overlay_callback_manager_impl.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request.h"

// Internal implementation of OverlayRequest.
class OverlayRequestImpl : public OverlayRequest,
                           public base::SupportsUserData {
 public:
  OverlayRequestImpl();
  ~OverlayRequestImpl() override;

  // OverlayRequest:
  OverlayCallbackManager* GetCallbackManager() override;
  web::WebState* GetQueueWebState() override;
  OverlayRequestId GetRequestId() const override;
  base::SupportsUserData* data() override;

 private:
  friend class OverlayRequestQueueImpl;

  // Setter for the return value for GetQueueWebState().  Called by the
  // OverlayRequestQueueImpl when the request is added.
  void set_queue_web_state(web::WebState* queue_web_state) {
    queue_web_state_ = queue_web_state;
  }

  const OverlayRequestId request_id_;
  raw_ptr<web::WebState, DanglingUntriaged> queue_web_state_ = nullptr;
  OverlayCallbackManagerImpl callback_manager_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_REQUEST_IMPL_H_
