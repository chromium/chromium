// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"

#import <utility>

#import "base/check.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/form_import/addresses/autofill_save_update_address_profile_delegate_ios.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type_util.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"

namespace autofill_address_profile_infobar_overlays {

SaveAddressProfileModalRequestConfig::SaveAddressProfileModalRequestConfig(
    InfoBarIOS* infobar) {
  DCHECK(infobar);
  infobar_ = infobar->AsWeakPtr();
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
    update_modal_description_ = delegate->GetSubtitle();
    is_profile_a_home_profile_ = delegate->IsOriginalProfileHomeProfile();
    is_profile_a_work_profile_ = delegate->IsOriginalProfileWorkProfile();
  }

  current_address_profile_saved_ = infobar->accepted();
  is_migration_to_account_ = delegate->IsMigrationToAccount();
  user_email_ = delegate->UserAccountEmail();
  is_profile_an_account_profile_ = delegate->IsProfileAnAccountProfile();
  profile_description_for_migration_prompt_ =
      delegate->GetProfileDescriptionForMigrationPrompt();
}

SaveAddressProfileModalRequestConfig::~SaveAddressProfileModalRequestConfig() =
    default;

bool SaveAddressProfileModalRequestConfig::IsUpdateModal() const {
  if (!infobar_) {
    return false;
  }

  return static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
             infobar_->delegate())
      ->GetOriginalProfile();
}

void SaveAddressProfileModalRequestConfig::StoreProfileDiff(
    const std::vector<autofill::ProfileValueDifference>& profile_diff) {
  // TODO(crbug.com/481234059): Convert this to CHECK after investigation.
  // Based of hypothesis in crbug.com/477044258, `GetProfileDifferenceForUi` is
  // returning empty.
  DUMP_WILL_BE_CHECK(!profile_diff.empty());
  for (const auto& row : profile_diff) {
    [profile_diff_
        setObject:@[
          base::SysUTF16ToNSString(row.first_value),
          base::SysUTF16ToNSString(row.second_value)
        ]
           forKey:[NSNumber numberWithInt:std::to_underlying(row.type)]];
  }
}

const autofill::AutofillProfile*
SaveAddressProfileModalRequestConfig::GetProfile() {
  if (!infobar_) {
    return nullptr;
  }

  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
          infobar_->delegate());
  return delegate->GetProfile();
}

void SaveAddressProfileModalRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_.get()),
      InfobarOverlayType::kModal, false);
}

}  // namespace autofill_address_profile_infobar_overlays
