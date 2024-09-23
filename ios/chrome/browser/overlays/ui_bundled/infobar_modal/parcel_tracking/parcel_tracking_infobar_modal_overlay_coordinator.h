// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_PARCEL_TRACKING_PARCEL_TRACKING_INFOBAR_MODAL_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_PARCEL_TRACKING_PARCEL_TRACKING_INFOBAR_MODAL_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/infobars/modals/parcel_tracking/infobar_parcel_tracking_presenter.h"

// A coordinator that displays the parcel tracking infobar modal UI.
@interface ParcelTrackingInfobarModalOverlayCoordinator
    : InfobarModalOverlayCoordinator <InfobarParcelTrackingPresenter>
@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_PARCEL_TRACKING_PARCEL_TRACKING_INFOBAR_MODAL_OVERLAY_COORDINATOR_H_
