// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_mediator.h"

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

// Shows success confirmation in the modal else dismisses the modal if card save
// is not successful.
- (void)creditCardUploadCompleted:(BOOL)card_saved;

// Instructs AutofillSaveCardInfoBarDelegateIOS that modal is not presenting,
// sets `CreditCardUploadCompletionCallback` to null and stops the overlay.
- (void)dismissOverlay;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
