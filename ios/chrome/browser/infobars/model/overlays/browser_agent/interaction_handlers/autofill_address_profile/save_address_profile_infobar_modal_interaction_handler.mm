// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_interaction_handler.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/form_import/addresses/autofill_save_update_address_profile_delegate_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_overlay_request_callback_installer.h"

SaveAddressProfileInfobarModalInteractionHandler::
    SaveAddressProfileInfobarModalInteractionHandler() = default;

SaveAddressProfileInfobarModalInteractionHandler::
    ~SaveAddressProfileInfobarModalInteractionHandler() = default;

#pragma mark - Public

void SaveAddressProfileInfobarModalInteractionHandler::PerformMainAction(
    InfoBarIOS* infobar) {
  infobar->set_accepted(GetInfoBarDelegate(infobar)->Accept());
}

void SaveAddressProfileInfobarModalInteractionHandler::InfobarVisibilityChanged(
    InfoBarIOS* infobar,
    bool visible) {
  GetInfoBarDelegate(infobar)->set_is_infobar_visible(visible);
}

void SaveAddressProfileInfobarModalInteractionHandler::CancelModal(
    InfoBarIOS* infobar) {
  GetInfoBarDelegate(infobar)->Cancel();
}

void SaveAddressProfileInfobarModalInteractionHandler::NoThanksWasPressed(
    InfoBarIOS* infobar) {
  GetInfoBarDelegate(infobar)->Never();
}

#pragma mark - Private

std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
SaveAddressProfileInfobarModalInteractionHandler::CreateModalInstaller() {
  return std::make_unique<
      SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller>(this);
}

autofill::AutofillSaveUpdateAddressProfileDelegateIOS*
SaveAddressProfileInfobarModalInteractionHandler::GetInfoBarDelegate(
    InfoBarIOS* infobar) {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      autofill::AutofillSaveUpdateAddressProfileDelegateIOS::
          FromInfobarDelegate(infobar->delegate());
  DCHECK(delegate);
  return delegate;
}
