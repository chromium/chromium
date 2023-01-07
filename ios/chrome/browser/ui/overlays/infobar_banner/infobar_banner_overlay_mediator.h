// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator.h"

@protocol InfobarBannerConsumer;

// Mediator superclass for configuring InfobarBannerConsumers.
@interface InfobarBannerOverlayMediator
    : OverlayRequestMediator <InfobarBannerDelegate>

// The consumer to be updated by this mediator.  Setting to a new value updates
// the new consumer.
@property(nonatomic, weak) id<InfobarBannerConsumer> consumer;

// Indicates to the mediator to do any cleanup work in response to a banner
// dismissal.
- (void)finishDismissal;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_
