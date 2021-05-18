// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill_address_profile_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(SaveAddressProfileModalRequestConfig);

SaveAddressProfileModalRequestConfig::SaveAddressProfileModalRequestConfig(
    InfoBarIOS* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
          infobar_->delegate());

  address_ = delegate->GetEnvelopeStyleAddress();
  emailAddress_ = delegate->GetEmailAddress();
  phoneNumber_ = delegate->GetPhoneNumber();
  current_address_profile_saved_ = infobar->accepted();
  profile_diff_ = [[NSMutableDictionary alloc] init];

  if (IsUpdateModal()) {
    StoreProfileDiff(delegate->GetProfileDiff());
    update_modal_description_ = delegate->GetDescription();
  }

  current_address_profile_saved_ = infobar->accepted();
}

SaveAddressProfileModalRequestConfig::~SaveAddressProfileModalRequestConfig() =
    default;

bool SaveAddressProfileModalRequestConfig::IsUpdateModal() const {
  return static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
             infobar_->delegate())
      ->GetOriginalProfile();
}

void SaveAddressProfileModalRequestConfig::StoreProfileDiff(
    const base::flat_map<autofill::ServerFieldType,
                         std::pair<std::u16string, std::u16string>>& diff_map) {
  for (const auto& row : diff_map) {
    [profile_diff_
        setObject:@[
          base::SysUTF16ToNSString(row.second.first),
          base::SysUTF16ToNSString(row.second.second)
        ]
           forKey:[NSNumber
                      numberWithInt:AutofillUITypeFromAutofillType(row.first)]];
  }
}

NSDictionary* SaveAddressProfileModalRequestConfig::GetProfileInfo() {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
          infobar_->delegate());
  NSMutableDictionary* items = [[NSMutableDictionary alloc] init];
  for (const auto& type : GetAutofillTypeForProfileEdit()) {
    [items setObject:base::SysUTF16ToNSString(delegate->GetProfileInfo(type))
              forKey:[NSNumber
                         numberWithInt:AutofillUITypeFromAutofillType(type)]];
  }
  return items;
}

void SaveAddressProfileModalRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, infobar_, InfobarOverlayType::kModal, false);
}

}  // namespace autofill_address_profile_infobar_overlays
