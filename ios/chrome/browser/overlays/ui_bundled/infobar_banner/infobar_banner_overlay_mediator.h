// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator.h"

namespace feature_engagement {
class Tracker;
}

@protocol InfobarBannerConsumer;

// Mediator superclass for configuring InfobarBannerConsumers.
@interface InfobarBannerOverlayMediator
    : OverlayRequestMediator <InfobarBannerDelegate>

// The consumer to be updated by this mediator.  Setting to a new value updates
// the new consumer.
@property(nonatomic, weak) id<InfobarBannerConsumer> consumer;

// Feature engagement tracker for notifying promo events.
@property(nonatomic, assign) feature_engagement::Tracker* engagementTracker;

// Indicates to the mediator to do any cleanup work in response to a banner
// dismissal.
- (void)finishDismissal;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_
