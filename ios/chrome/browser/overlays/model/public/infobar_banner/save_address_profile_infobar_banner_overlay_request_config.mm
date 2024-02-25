// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"

#import "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace autofill_address_profile_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(SaveAddressProfileBannerRequestConfig);

SaveAddressProfileBannerRequestConfig::SaveAddressProfileBannerRequestConfig(
    infobars::InfoBar* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      autofill::AutofillSaveUpdateAddressProfileDelegateIOS::
          FromInfobarDelegate(infobar_->delegate());
  message_text_ = delegate->GetMessageText();
  button_label_text_ = delegate->GetMessageActionText();
  description_ = delegate->GetDescription();
  is_update_banner_ = delegate->GetOriginalProfile() ? true : false;
  is_migration_to_account_ = delegate->IsMigrationToAccount();
  is_profile_an_account_profile_ = delegate->IsProfileAnAccountProfile();
}

SaveAddressProfileBannerRequestConfig::
    ~SaveAddressProfileBannerRequestConfig() = default;

void SaveAddressProfileBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}

}  // namespace autofill_address_profile_infobar_overlays
