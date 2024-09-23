// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_MEDIATOR_H_

#import <Foundation/Foundation.h>

class OverlayRequest;
@protocol OverlayRequestMediatorDelegate;
class OverlayRequestSupport;

// Mediator used to configure overlay UI consumers using an OverlayRequest.
// Subclasses should use the request passed on initialization to set up overlay
// UI via consumer protocols specific to that config type.
@interface OverlayRequestMediator : NSObject

// Returns the request support for this mediator.  Must return a non-null value.
@property(class, nonatomic, readonly)
    const OverlayRequestSupport* requestSupport;

// The request passed on initialization.  Reset to nullptr if the request is
// cancelled while its overlay UI is still visible.
@property(nonatomic, readonly) OverlayRequest* request;

// The delegate.
@property(nonatomic, weak) id<OverlayRequestMediatorDelegate> delegate;

// Returns an OverlayRequestSupport that only supports requests created with
// ConfigType.
+ (const OverlayRequestSupport*)requestSupport;

// Initializer for a mediator that sets ups its consumer with `request`'s
// config.
- (instancetype)initWithRequest:(OverlayRequest*)request
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

// Delegate for the mediator used to stop the overlay when user interaction
// should trigger dismissal.
@protocol OverlayRequestMediatorDelegate <NSObject>

// Stops the overlay UI.
- (void)stopOverlayForMediator:(OverlayRequestMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_MEDIATOR_H_
