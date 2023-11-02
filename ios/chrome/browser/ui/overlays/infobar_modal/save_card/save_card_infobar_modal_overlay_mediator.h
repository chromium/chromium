// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_mediator.h"

#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_delegate.h"

@protocol InfobarSaveCardModalConsumer;
@protocol SaveCardInfobarModalOverlayMediatorDelegate;

// Mediator that configures the modal UI for a passwords infobar.
@interface SaveCardInfobarModalOverlayMediator
    : InfobarModalOverlayMediator <InfobarSaveCardModalDelegate>

// The consumer that is configured by this mediator.  Setting to a new value
// configures the new consumer.
@property(nonatomic) id<InfobarSaveCardModalConsumer> consumer;

// The delegate of this specific mediator.
@property(nonatomic) id<SaveCardInfobarModalOverlayMediatorDelegate>
    save_card_delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
