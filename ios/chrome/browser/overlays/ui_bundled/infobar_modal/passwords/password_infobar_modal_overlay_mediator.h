// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_PASSWORDS_PASSWORD_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_PASSWORDS_PASSWORD_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/infobars/modals/infobar_password_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_password_modal_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_mediator.h"

// Mediator that configures the modal UI for a passwords infobar.
@interface PasswordInfobarModalOverlayMediator
    : InfobarModalOverlayMediator <InfobarPasswordModalDelegate>

// The consumer that is configured by this mediator.  Setting to a new value
// configures the new consumer.
@property(nonatomic) id<InfobarPasswordModalConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_PASSWORDS_PASSWORD_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
