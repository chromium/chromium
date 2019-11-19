// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/overlay_request_cancel_handler.h"

#include "base/logging.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OverlayRequestCancelHandler::OverlayRequestCancelHandler(
    OverlayRequest* request,
    OverlayRequestQueue* queue)
    : request_(request), queue_(queue) {
  DCHECK(request_);
  DCHECK(queue_);
}

void OverlayRequestCancelHandler::CancelRequest() {
  queue_->CancelRequest(request_);
}
