// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_

#include <memory>
#include <string>

#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockAutofillSaveUpdateAddressProfileDelegateIOS
    : public autofill::AutofillSaveUpdateAddressProfileDelegateIOS {
 public:
  MockAutofillSaveUpdateAddressProfileDelegateIOS(
      const autofill::AutofillProfile& profile,
      const autofill::AutofillProfile* original_profile,
      const std::string& locale,
      autofill::AutofillClient::AddressProfileSavePromptCallback callback);
  ~MockAutofillSaveUpdateAddressProfileDelegateIOS() override;

  MOCK_METHOD0(Accept, bool());
  MOCK_METHOD0(EditAccepted, void());
  MOCK_METHOD0(Never, bool());
};

class MockAutofillSaveUpdateAddressProfileDelegateIOSFactory {
 public:
  MockAutofillSaveUpdateAddressProfileDelegateIOSFactory();
  ~MockAutofillSaveUpdateAddressProfileDelegateIOSFactory();

  static std::unique_ptr<MockAutofillSaveUpdateAddressProfileDelegateIOS>
  CreateMockAutofillSaveUpdateAddressProfileDelegateIOSFactory(
      autofill::AutofillProfile profile);

 private:
  autofill::AutofillProfile profile_{
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode};
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_
