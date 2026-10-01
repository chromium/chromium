// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_FACTORY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_FACTORY_H_

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"

class OverlayRequest;
class OverlayRequestSupport;

// Category on InfobarBannerOverlayMediator to create mediator instances
// for overlay requests.
@interface InfobarBannerOverlayMediator (Factory)

// Returns the aggregate request support for all supported mediator subclasses.
+ (const OverlayRequestSupport*)requestSupport;

// Returns an instance of the InfobarBannerOverlayMediator subclass supporting
// `request`.
+ (instancetype)mediatorForRequest:(OverlayRequest*)request;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_FACTORY_H_
