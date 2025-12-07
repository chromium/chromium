// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/settings_autofill_edit_profile_bottom_sheet_handler.h"

#import <Foundation/Foundation.h>

#import "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"

@implementation SettingsAutofillEditProfileBottomSheetHandler {
  // The account email of the signed-in user. Will be `nil` if there is no
  // signed-in user.
  NSString* _userEmail;

  // Used to manage addresses.
  raw_ptr<autofill::AddressDataManager> _addressDataManager;
}

- (instancetype)initWithAddressDataManager:
                    (autofill::AddressDataManager*)addressDataManager
                                 userEmail:(NSString*)userEmail {
  if (self) {
    _userEmail = userEmail;

    CHECK(addressDataManager);
    _addressDataManager = addressDataManager;
  }
  return self;
}

#pragma mark - AutofillEditProfileHandler

- (void)didCancelSheetView {
  // No-op.
}

- (void)didSaveProfile:(autofill::AutofillProfile*)profile {
  _addressDataManager->AddProfile(*profile);
}

- (BOOL)isMigrationToAccount {
  // Since a new address is being added, and only existing addresses can be
  // migrated, this method always returns `NO`.
  return NO;
}

- (std::unique_ptr<autofill::AutofillProfile>)autofillProfile {
  // Since this is creating a new (empty) address, use the app's locale country
  // code as the default value.
  autofill::AddressCountryCode countryCode =
      _addressDataManager->GetDefaultCountryCodeForNewAddress();

  std::unique_ptr<autofill::AutofillProfile> autofillProfile =
      std::make_unique<autofill::AutofillProfile>(countryCode);

  if (_addressDataManager->IsEligibleForAddressAccountStorage()) {
    autofillProfile = std::make_unique<autofill::AutofillProfile>(
        autofillProfile->ConvertToAccountProfile());
  }

  return autofillProfile;
}

- (AutofillSaveProfilePromptMode)saveProfilePromptMode {
  return AutofillSaveProfilePromptMode::kNewProfile;
}

- (NSString*)userEmail {
  return _userEmail;
}

- (BOOL)addingManualAddress {
  return YES;
}

@end
