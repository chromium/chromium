// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_INFOBAR_MODAL_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_INFOBAR_MODAL_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_presentation_handler.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator.h"

// A coordinator that displays infobar modal UI using OverlayPresenter.
@interface InfobarModalOverlayCoordinator
    : OverlayRequestCoordinator <InfobarModalPresentationHandler>

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_INFOBAR_MODAL_OVERLAY_COORDINATOR_H_
