// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_MODAL_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_MODAL_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_

#import <UIKit/UIKit.h>

#include <optional>

#import "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_config.h"

class InfoBarIOS;

namespace autofill_address_profile_infobar_overlays {

// Configuration object for OverlayRequests for the modal UI for an infobar with
// a AutofillSaveAddressProfilePromptDelegateMobile.
class SaveAddressProfileModalRequestConfig
    : public OverlayRequestConfig<SaveAddressProfileModalRequestConfig> {
 public:
  ~SaveAddressProfileModalRequestConfig() override;

  // Returns the envelope style address stored in `address_`..
  std::u16string address() const { return address_; }

  // Returns phone number stored in the `profile_`.
  std::u16string phoneNumber() const { return phoneNumber_; }

  // Returns email stored in the `profile_`.
  std::u16string emailAddress() const { return emailAddress_; }

  // Returns the original profile's description for display.
  std::u16string update_modal_description() const {
    return update_modal_description_;
  }

  std::u16string profile_description_for_migration_prompt() const {
    return profile_description_for_migration_prompt_;
  }

  // Returns `profile_diff_` containing the profile differences fetched from the
  // delegate.
  NSMutableDictionary<NSNumber*, NSArray*>* profile_diff() const {
    return profile_diff_;
  }

  // Profile to be saved.
  const autofill::AutofillProfile* GetProfile();

  // Whether the request is for the update address profile modal.
  bool IsUpdateModal() const;

  // Whether the current address profile is already saved.
  bool current_address_profile_saved() const {
    return current_address_profile_saved_;
  }

  bool is_migration_to_account() const { return is_migration_to_account_; }

  std::optional<std::u16string> user_email() const { return user_email_; }

  bool is_profile_an_account_profile() const {
    return is_profile_an_account_profile_;
  }

 private:
  OVERLAY_USER_DATA_SETUP(SaveAddressProfileModalRequestConfig);
  explicit SaveAddressProfileModalRequestConfig(InfoBarIOS* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // Computes `profile_diff_` based on the map of
  // profile difference data fetched from the delegate.
  void StoreProfileDiff(
      const std::vector<autofill::ProfileValueDifference>& profile_diff);

  // The InfoBar causing this modal.
  raw_ptr<InfoBarIOS> infobar_ = nullptr;

  // Configuration data extracted from `infobar_`'s save address profile
  // delegate.
  std::u16string address_;
  std::u16string emailAddress_;
  std::u16string phoneNumber_;

  // Configuration data extracted from `infobar_`'s update address profile
  // delegate.
  std::u16string update_modal_description_;
  // The key is AutofillUIType and the value consists of array
  // containing the delegate's profile and original_profile data corresponding
  // to the type.
  NSMutableDictionary<NSNumber*, NSArray*>* profile_diff_;

  // True if the address profile is saved.
  bool current_address_profile_saved_ = false;

  // Denotes that the profile will be saved to Google Account.
  bool is_migration_to_account_ = false;

  // Denotes that the profile is an account profile.
  bool is_profile_an_account_profile_ = false;

  // Denotes the email address of the signed-in account.
  std::optional<std::u16string> user_email_;

  // Denotes the profile description shown in the migration prompt.
  std::u16string profile_description_for_migration_prompt_;
};

}  // namespace autofill_address_profile_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_MODAL_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_
