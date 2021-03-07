// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_address_profile_delegate_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/bind.h"
#include "base/guid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"

MockAutofillSaveAddressProfileDelegateIOS::
    MockAutofillSaveAddressProfileDelegateIOS(
        const autofill::AutofillProfile& profile,
        autofill::AutofillClient::AddressProfileSavePromptCallback callback)
    : AutofillSaveAddressProfileDelegateIOS(profile, std::move(callback)) {}

MockAutofillSaveAddressProfileDelegateIOS::
    ~MockAutofillSaveAddressProfileDelegateIOS() = default;

#pragma mark - MockAutofillSaveAddressProfileDelegateIOSFactory

MockAutofillSaveAddressProfileDelegateIOSFactory::
    MockAutofillSaveAddressProfileDelegateIOSFactory()
    : profile_(base::GenerateGUID(), "https://www.example.com/") {}

MockAutofillSaveAddressProfileDelegateIOSFactory::
    ~MockAutofillSaveAddressProfileDelegateIOSFactory() = default;

std::unique_ptr<MockAutofillSaveAddressProfileDelegateIOS>
MockAutofillSaveAddressProfileDelegateIOSFactory::
    CreateMockAutofillSaveAddressProfileDelegateIOSFactory(
        autofill::AutofillProfile profile) {
  return std::make_unique<MockAutofillSaveAddressProfileDelegateIOS>(
      profile, autofill::AutofillClient::AddressProfileSavePromptCallback());
}
