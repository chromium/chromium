// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_banner_interaction_handler.h"

#import "base/check.h"
#import "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_banner_overlay_request_callback_installer.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_card_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_card_infobar_overlays::SaveCardBannerRequestConfig;

#pragma mark - InfobarBannerInteractionHandler

SaveCardInfobarBannerInteractionHandler::
    SaveCardInfobarBannerInteractionHandler()
    : InfobarBannerInteractionHandler(
          SaveCardBannerRequestConfig::RequestSupport()) {}

SaveCardInfobarBannerInteractionHandler::
    ~SaveCardInfobarBannerInteractionHandler() = default;

void SaveCardInfobarBannerInteractionHandler::SaveCredentials(
    InfoBarIOS* infobar,
    std::u16string cardholder_name,
    std::u16string expiration_date_month,
    std::u16string expiration_date_year) {
  infobar->set_accepted(GetInfobarDelegate(infobar)->UpdateAndAccept(
      cardholder_name, expiration_date_month, expiration_date_year));
}

#pragma mark - Private

std::unique_ptr<InfobarBannerOverlayRequestCallbackInstaller>
SaveCardInfobarBannerInteractionHandler::CreateBannerInstaller() {
  return std::make_unique<SaveCardInfobarBannerOverlayRequestCallbackInstaller>(
      this);
}

autofill::AutofillSaveCardInfoBarDelegateMobile*
SaveCardInfobarBannerInteractionHandler::GetInfobarDelegate(
    InfoBarIOS* infobar) {
  autofill::AutofillSaveCardInfoBarDelegateMobile* delegate =
      autofill::AutofillSaveCardInfoBarDelegateMobile::FromInfobarDelegate(
          infobar->delegate());
  DCHECK(delegate);
  return delegate;
}
