// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_ALERT_OVERLAY_COORDINATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_ALERT_OVERLAY_COORDINATOR_SUBCLASSING_H_

#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_coordinator.h"

@class AlertViewController;
@class AlertOverlayMediator;

// Category that allows subclasses to update UI using the OverlayRequest
// configuration.
@interface AlertOverlayCoordinator (Subclassing)

// Subclasses must override this selector to create a mediator that will
// configure the alert using the OverlayRequest's config.
- (AlertOverlayMediator*)newMediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_COMMON_ALERTS_ALERT_OVERLAY_COORDINATOR_SUBCLASSING_H_
