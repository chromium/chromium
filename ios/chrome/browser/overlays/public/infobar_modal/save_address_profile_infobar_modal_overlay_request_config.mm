// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_save_address_profile_delegate_ios.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace save_address_profile_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(SaveAddressProfileModalRequestConfig);

SaveAddressProfileModalRequestConfig::SaveAddressProfileModalRequestConfig(
    InfoBarIOS* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  autofill::AutofillSaveAddressProfileDelegateIOS* delegate =
      static_cast<autofill::AutofillSaveAddressProfileDelegateIOS*>(
          infobar_->delegate());
  profile_ = delegate->GetProfile();
  current_address_profile_saved_ = infobar->accepted();
}

SaveAddressProfileModalRequestConfig::~SaveAddressProfileModalRequestConfig() =
    default;

std::u16string SaveAddressProfileModalRequestConfig::GetProfileName() const {
  return profile_->GetRawInfo(autofill::NAME_FULL);
}

std::u16string SaveAddressProfileModalRequestConfig::GetProfileAddressLine1()
    const {
  return profile_->GetRawInfo(autofill::ADDRESS_HOME_LINE1);
}

std::u16string SaveAddressProfileModalRequestConfig::GetProfileAddressLine2()
    const {
  return profile_->GetRawInfo(autofill::ADDRESS_HOME_LINE2);
}

std::u16string SaveAddressProfileModalRequestConfig::GetProfileCity() const {
  return profile_->GetRawInfo(autofill::ADDRESS_HOME_CITY);
}

std::u16string SaveAddressProfileModalRequestConfig::GetProfileState() const {
  return profile_->GetRawInfo(autofill::ADDRESS_HOME_STATE);
}

std::u16string SaveAddressProfileModalRequestConfig::GetProfileCountry() const {
  // TODO(crbug.com/1167062): Display country name instead of country code.
  return profile_->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY);
}

std::u16string SaveAddressProfileModalRequestConfig::GetProfileZip() const {
  return profile_->GetRawInfo(autofill::ADDRESS_HOME_ZIP);
}

std::u16string SaveAddressProfileModalRequestConfig::GetProfilePhone() const {
  return profile_->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER);
}

std::u16string SaveAddressProfileModalRequestConfig::GetProfileEmail() const {
  return profile_->GetRawInfo(autofill::EMAIL_ADDRESS);
}

void SaveAddressProfileModalRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, infobar_, InfobarOverlayType::kModal, false);
}

}  // namespace save_address_profile_infobar_overlays
