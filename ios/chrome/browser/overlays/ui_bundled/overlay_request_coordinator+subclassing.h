// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_COORDINATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_COORDINATOR_SUBCLASSING_H_

#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator.h"

@class OverlayRequestMediator;

// Interface for concrete subclasses of OverlayRequestCoordinator.
@interface OverlayRequestCoordinator (Subclassing)

// Whether the coordinator has been started.
@property(nonatomic, assign, getter=isStarted) BOOL started;

// The mediator used to configure the overlay UI.
@property(nonatomic, strong) OverlayRequestMediator* mediator;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_COORDINATOR_SUBCLASSING_H_
