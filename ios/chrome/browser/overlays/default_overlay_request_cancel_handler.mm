// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/default_overlay_request_cancel_handler.h"

#include "base/logging.h"
#import "ios/web/public/navigation/navigation_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    : cancel_handler_(cancel_handler), scoped_observer_(this) {
  DCHECK(cancel_handler);
  DCHECK(web_state);
  scoped_observer_.Add(web_state);
}

DefaultOverlayRequestCancelHandler::NavigationHelper::~NavigationHelper() =
    default;

void DefaultOverlayRequestCancelHandler::NavigationHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->HasCommitted() &&
      !navigation_context->IsSameDocument()) {
    cancel_handler_->Cancel();
  }
}

void DefaultOverlayRequestCancelHandler::NavigationHelper::RenderProcessGone(
    web::WebState* web_state) {
  cancel_handler_->Cancel();
}

void DefaultOverlayRequestCancelHandler::NavigationHelper::WebStateDestroyed(
    web::WebState* web_state) {
  cancel_handler_->Cancel();
  scoped_observer_.RemoveAll();
}
