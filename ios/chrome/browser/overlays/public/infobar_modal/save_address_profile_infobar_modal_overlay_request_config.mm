// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#include "ios/chrome/browser/application_context.h"
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
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
          infobar_->delegate());

  address_ = delegate->GetEnvelopeStyleAddress(
      GetApplicationContext()->GetApplicationLocale());
  emailAddress_ = delegate->GetEmailAddress();
  phoneNumber_ = delegate->GetPhoneNumber();
  current_address_profile_saved_ = infobar->accepted();
}

SaveAddressProfileModalRequestConfig::~SaveAddressProfileModalRequestConfig() =
    default;

std::u16string SaveAddressProfileModalRequestConfig::GetAddress() const {
  return address_;
}

std::u16string SaveAddressProfileModalRequestConfig::GetPhoneNumber() const {
  return phoneNumber_;
}

std::u16string SaveAddressProfileModalRequestConfig::GetEmailAddress() const {
  return emailAddress_;
}

void SaveAddressProfileModalRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, infobar_, InfobarOverlayType::kModal, false);
}

}  // namespace save_address_profile_infobar_overlays
