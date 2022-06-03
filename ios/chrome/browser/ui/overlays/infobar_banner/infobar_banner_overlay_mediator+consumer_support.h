// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_CONSUMER_SUPPORT_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_CONSUMER_SUPPORT_H_

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator.h"

// Category used by the InfobarBannerOverlayMediator superclass in order to
// configure its consumer.
@interface InfobarBannerOverlayMediator (ConsumerSupport)

// Sets up the banner consumer using the configuration information from the
// mediator's OverlayRequest.  Subclasses must implement.
- (void)configureConsumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_CONSUMER_SUPPORT_H_
