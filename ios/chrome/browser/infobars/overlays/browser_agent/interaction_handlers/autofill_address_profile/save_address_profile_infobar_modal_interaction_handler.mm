// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_interaction_handler.h"

#include "components/autofill/core/browser/autofill_save_address_profile_delegate_ios.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_overlay_request_callback_installer.h"
#include "ios/chrome/browser/main/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SaveAddressProfileInfobarModalInteractionHandler::
    SaveAddressProfileInfobarModalInteractionHandler(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
}

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
  if (!visible && !infobar->accepted()) {
    // Inform the delegate that the modal has been dismissed.
    GetInfoBarDelegate(infobar)->set_modal_is_dismissed_to_true();
    GetInfoBarDelegate(infobar)->InfoBarDismissed();
  }
}

void SaveAddressProfileInfobarModalInteractionHandler::
    PresentAddressProfileSettings(InfoBarIOS* infobar) {
  // TODO(crbug.com/1167062): Open Address Profile settings.
}

#pragma mark - Private

std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
SaveAddressProfileInfobarModalInteractionHandler::CreateModalInstaller() {
  return std::make_unique<
      SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller>(this);
}

autofill::AutofillSaveAddressProfileDelegateIOS*
SaveAddressProfileInfobarModalInteractionHandler::GetInfoBarDelegate(
    InfoBarIOS* infobar) {
  autofill::AutofillSaveAddressProfileDelegateIOS* delegate =
      autofill::AutofillSaveAddressProfileDelegateIOS::FromInfobarDelegate(
          infobar->delegate());
  DCHECK(delegate);
  return delegate;
}
