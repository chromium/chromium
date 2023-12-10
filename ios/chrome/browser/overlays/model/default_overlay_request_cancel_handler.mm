// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/default_overlay_request_cancel_handler.h"

#import "base/check.h"
#import "ios/web/public/navigation/navigation_context.h"

DefaultOverlayRequestCancelHandler::DefaultOverlayRequestCancelHandler(
    OverlayRequest* request,
    OverlayRequestQueue* queue,
    web::WebState* web_state)
    : OverlayRequestCancelHandler(request, queue),
      navigation_helper_(this, web_state) {}

DefaultOverlayRequestCancelHandler::~DefaultOverlayRequestCancelHandler() =
    default;

void DefaultOverlayRequestCancelHandler::Cancel() {
  CancelRequest();
}

DefaultOverlayRequestCancelHandler::NavigationHelper::NavigationHelper(
    DefaultOverlayRequestCancelHandler* cancel_handler,
    web::WebState* web_state)
    : cancel_handler_(cancel_handler) {
  DCHECK(cancel_handler);
  DCHECK(web_state);
  scoped_observation_.Observe(web_state);
}

DefaultOverlayRequestCancelHandler::NavigationHelper::~NavigationHelper() =
    default;

void DefaultOverlayRequestCancelHandler::NavigationHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->HasCommitted() &&
      !navigation_context->IsSameDocument()) {
    cancel_handler_->Cancel();
    // The cancel handler is destroyed after Cancel(), so no code can be added
    // after this call.
  }
}

void DefaultOverlayRequestCancelHandler::NavigationHelper::RenderProcessGone(
    web::WebState* web_state) {
  cancel_handler_->Cancel();
  // The cancel handler is destroyed after Cancel(), so no code can be added
  // after this call.
}

void DefaultOverlayRequestCancelHandler::NavigationHelper::WebStateDestroyed(
    web::WebState* web_state) {
  scoped_observation_.Reset();
  cancel_handler_->Cancel();
  // The cancel handler is destroyed after Cancel(), so no code can be added
  // after this call.
}
