// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_update_address_profile_delegate_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/bind.h"
#import "base/guid.h"
#import "components/autofill/core/browser/autofill_test_utils.h"

MockAutofillSaveUpdateAddressProfileDelegateIOS::
    MockAutofillSaveUpdateAddressProfileDelegateIOS(
        const autofill::AutofillProfile& profile,
        const autofill::AutofillProfile* original_profile,
        const std::string& locale,
        autofill::AutofillClient::AddressProfileSavePromptCallback callback)
    : AutofillSaveUpdateAddressProfileDelegateIOS(profile,
                                                  original_profile,
                                                  locale,
                                                  std::move(callback)) {}

MockAutofillSaveUpdateAddressProfileDelegateIOS::
    ~MockAutofillSaveUpdateAddressProfileDelegateIOS() = default;

#pragma mark - MockAutofillSaveUpdateAddressProfileDelegateIOSFactory

MockAutofillSaveUpdateAddressProfileDelegateIOSFactory::
    MockAutofillSaveUpdateAddressProfileDelegateIOSFactory()
    : profile_(base::GenerateGUID(), "https://www.example.com/") {}

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
