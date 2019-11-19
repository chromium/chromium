// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator.h"

class OverlayRequest;
@protocol HTTPAuthDialogOverlayMediatorDataSource;

// Mediator object that uses a HTTPAuthOverlayRequestConfig to set up the UI for
// an HTTP authentication dialog.
@interface HTTPAuthDialogOverlayMediator : AlertOverlayMediator

// Designated initializer for a mediator that uses |request|'s configuration to
// set up an AlertConsumer.
- (instancetype)initWithRequest:(OverlayRequest*)request
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_OVERLAY_MEDIATOR_H_
