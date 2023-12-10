// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/overlay_request_cancel_handler.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"

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
