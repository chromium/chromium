// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_MEDIATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_MEDIATOR_SUBCLASSING_H_

#import "ios/chrome/browser/ui/overlays/overlay_request_mediator.h"

#include <memory>

#include "ios/chrome/browser/overlays/public/overlay_response.h"

// Exposes shared functionality for OverlayRequestMediator subclasses.
@interface OverlayRequestMediator (Subclassing)

// Dispatches |response| through the mediator's request callback manager.  Does
// nothing if the request has been cancelled.
- (void)dispatchResponse:(std::unique_ptr<OverlayResponse>)response;

// Instructs the delegate to stop the overlay.
- (void)dismissOverlay;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_MEDIATOR_SUBCLASSING_H_
