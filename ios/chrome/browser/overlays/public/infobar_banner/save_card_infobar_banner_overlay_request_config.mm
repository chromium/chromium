// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/save_card_infobar_banner_overlay_request_config.h"

#import "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace save_card_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(SaveCardBannerRequestConfig);

SaveCardBannerRequestConfig::SaveCardBannerRequestConfig(
    infobars::InfoBar* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  autofill::AutofillSaveCardInfoBarDelegateMobile* delegate =
      autofill::AutofillSaveCardInfoBarDelegateMobile::FromInfobarDelegate(
          infobar_->delegate());
  message_text_ = delegate->GetMessageText();
  card_label_ = delegate->card_label();
  cardholder_name_ = delegate->cardholder_name();
  expiration_date_month_ = delegate->expiration_date_month();
  expiration_date_year_ = delegate->expiration_date_year();
  button_label_text_ =
      delegate->is_for_upload()
          ? l10n_util::GetStringUTF16(IDS_IOS_AUTOFILL_SAVE_ELLIPSIS)
          : delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK);
  should_upload_credentials_ = delegate->is_for_upload();
}

SaveCardBannerRequestConfig::~SaveCardBannerRequestConfig() = default;

void SaveCardBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}

}  // namespace save_card_infobar_overlays
