// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_APP_LAUNCHER_APP_LAUNCHER_ALERT_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_APP_LAUNCHER_APP_LAUNCHER_ALERT_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator.h"

class OverlayRequest;

// Mediator object that uses a AppLauncherAlertOverlayRequestConfig to set up
// the UI for an alert notifying the user that a navigation will open an
// external app.
@interface AppLauncherAlertOverlayMediator : AlertOverlayMediator

// Designated initializer for a mediator that uses |request|'s configuration to
// set up an AlertConsumer.
- (instancetype)initWithRequest:(OverlayRequest*)request
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_APP_LAUNCHER_APP_LAUNCHER_ALERT_OVERLAY_MEDIATOR_H_
