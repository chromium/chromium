// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_modal_interaction_handler.h"

#import "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_modal_overlay_request_callback_installer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SaveCardInfobarModalInteractionHandler::
    SaveCardInfobarModalInteractionHandler() = default;

SaveCardInfobarModalInteractionHandler::
    ~SaveCardInfobarModalInteractionHandler() = default;

#pragma mark - Public

void SaveCardInfobarModalInteractionHandler::UpdateCredentials(
    InfoBarIOS* infobar,
    std::u16string cardholder_name,
    std::u16string expiration_date_month,
    std::u16string expiration_date_year) {
  infobar->set_accepted(GetInfoBarDelegate(infobar)->UpdateAndAccept(
      cardholder_name, expiration_date_month, expiration_date_year));
}

void SaveCardInfobarModalInteractionHandler::LoadURL(InfoBarIOS* infobar,
                                                     GURL url) {
  GetInfoBarDelegate(infobar)->OnLegalMessageLinkClicked(url);
}

void SaveCardInfobarModalInteractionHandler::PerformMainAction(
    InfoBarIOS* infobar) {
  NOTREACHED() << "SaveCard does not use standard Infobar Accept action.";
}

#pragma mark - Private

std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
SaveCardInfobarModalInteractionHandler::CreateModalInstaller() {
  return std::make_unique<SaveCardInfobarModalOverlayRequestCallbackInstaller>(
      this);
}

autofill::AutofillSaveCardInfoBarDelegateMobile*
SaveCardInfobarModalInteractionHandler::GetInfoBarDelegate(
    InfoBarIOS* infobar) {
  autofill::AutofillSaveCardInfoBarDelegateMobile* delegate =
      autofill::AutofillSaveCardInfoBarDelegateMobile::FromInfobarDelegate(
          infobar->delegate());
  DCHECK(delegate);
  return delegate;
}
