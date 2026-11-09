// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/non_modal_signin_promo_commands.h"

namespace feature_engagement {
class Tracker;
}

// Mediator superclass for configuring InfobarBannerConsumers.
@interface InfobarBannerOverlayMediator
    : OverlayRequestMediator <InfobarBannerDelegate>

// The consumer to be updated by this mediator.  Setting to a new value updates
// the new consumer.
@property(nonatomic, weak) id<InfobarBannerConsumer> consumer;

// Feature engagement tracker for notifying promo events.
@property(nonatomic, assign) feature_engagement::Tracker* engagementTracker;

// Handler for executing NonModalSignInPromoCommands.
@property(nonatomic, weak) id<NonModalSignInPromoCommands>
    nonModalSignInPromoHandler;

// Configures command handlers and dependencies using `dispatcher`. Subclasses
// can override to inject specific command handlers.
- (void)configureDependenciesWithDispatcher:(CommandDispatcher*)dispatcher
    NS_REQUIRES_SUPER;

// Indicates to the mediator to do any cleanup work in response to a banner
// dismissal.
- (void)finishDismissal;

// Disconnects the mediator.
- (void)disconnect NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_
