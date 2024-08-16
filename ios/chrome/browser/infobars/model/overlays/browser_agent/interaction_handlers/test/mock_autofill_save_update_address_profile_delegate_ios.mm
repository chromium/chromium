// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_update_address_profile_delegate_ios.h"

#import "base/functional/bind.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/autofill_test_utils.h"

MockAutofillSaveUpdateAddressProfileDelegateIOS::
    MockAutofillSaveUpdateAddressProfileDelegateIOS(
        const autofill::AutofillProfile& profile,
        const autofill::AutofillProfile* original_profile,
        const std::string& locale,
        autofill::AutofillClient::AddressProfileSavePromptCallback callback)
    : AutofillSaveUpdateAddressProfileDelegateIOS(
          profile,
          original_profile,
          /*syncing_user_email=*/std::nullopt,
          locale,
          /*is_migration_to_account=*/false,
          std::move(callback)) {}

MockAutofillSaveUpdateAddressProfileDelegateIOS::
    ~MockAutofillSaveUpdateAddressProfileDelegateIOS() = default;

#pragma mark - MockAutofillSaveUpdateAddressProfileDelegateIOSFactory

MockAutofillSaveUpdateAddressProfileDelegateIOSFactory::
    MockAutofillSaveUpdateAddressProfileDelegateIOSFactory() = default;

MockAutofillSaveUpdateAddressProfileDelegateIOSFactory::
    ~MockAutofillSaveUpdateAddressProfileDelegateIOSFactory() = default;

std::unique_ptr<MockAutofillSaveUpdateAddressProfileDelegateIOS>
MockAutofillSaveUpdateAddressProfileDelegateIOSFactory::
    CreateMockAutofillSaveUpdateAddressProfileDelegateIOSFactory(
        autofill::AutofillProfile profile) {
  return std::make_unique<MockAutofillSaveUpdateAddressProfileDelegateIOS>(
      profile, /*original_profile=*/nullptr, /*locale=*/"en-US",
      autofill::AutofillClient::AddressProfileSavePromptCallback());
}
