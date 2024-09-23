// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_PARCEL_TRACKING_PARCEL_TRACKING_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_PARCEL_TRACKING_PARCEL_TRACKING_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/infobars/modals/parcel_tracking/infobar_parcel_tracking_modal_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_mediator.h"

@protocol InfobarParcelTrackingModalConsumer;

// Mediator that configures the modal UI for a parcel tracking infobar.
@interface ParcelTrackingInfobarModalOverlayMediator
    : InfobarModalOverlayMediator <InfobarParcelTrackingModalDelegate>

// The consumer that is configured by this mediator.
@property(nonatomic) id<InfobarParcelTrackingModalConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_PARCEL_TRACKING_PARCEL_TRACKING_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
