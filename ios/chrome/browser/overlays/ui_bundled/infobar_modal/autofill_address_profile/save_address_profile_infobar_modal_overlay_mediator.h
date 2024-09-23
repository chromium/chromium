// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_AUTOFILL_ADDRESS_PROFILE_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_AUTOFILL_ADDRESS_PROFILE_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_mediator.h"

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_modal_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator_delegate.h"

namespace autofill {
class AutofillProfile;
}

@protocol InfobarEditAddressProfileModalConsumer;
@protocol InfobarSaveAddressProfileModalConsumer;

// Mediator that configures the modal UI for save address profile infobar.
@interface SaveAddressProfileInfobarModalOverlayMediator
    : InfobarModalOverlayMediator <InfobarSaveAddressProfileModalDelegate>

// The consumer that is configured by this mediator.  Setting to a new value
// configures the new consumer.
@property(nonatomic) id<InfobarSaveAddressProfileModalConsumer> consumer;

// The consumer that is configured by this mediator for the edit address
// profile.
@property(nonatomic) id<InfobarEditAddressProfileModalConsumer>
    editAddressConsumer;

// Delegate to communicate user actions to change the UI presentation.
@property(nonatomic) id<SaveAddressProfileInfobarModalOverlayMediatorDelegate>
    saveAddressProfileMediatorDelegate;

// Saves the autofill `profile` that was edited in the edit modal view.
- (void)saveEditedProfileWithProfileData:(autofill::AutofillProfile*)profile;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_AUTOFILL_ADDRESS_PROFILE_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
