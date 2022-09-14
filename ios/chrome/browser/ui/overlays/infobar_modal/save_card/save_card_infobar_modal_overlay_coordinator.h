// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator.h"

#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator_delegate.h"

// A coordinator that displays the Save Card infobar modal UI using
// OverlayPresenter.
@interface SaveCardInfobarModalOverlayCoordinator
    : InfobarModalOverlayCoordinator <
          SaveCardInfobarModalOverlayMediatorDelegate>
@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_COORDINATOR_H_
