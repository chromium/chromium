// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_interaction_handler.h"

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_update_address_profile_delegate_ios.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// Test fixture for SaveAddressProfileInfobarModalInteractionHandler.
class SaveAddressProfileInfobarModalInteractionHandlerTest
    : public PlatformTest {
 public:
  SaveAddressProfileInfobarModalInteractionHandlerTest() : delegate_factory_() {
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSaveAutofillAddressProfile,
        MockAutofillSaveUpdateAddressProfileDelegateIOSFactory::
            CreateMockAutofillSaveUpdateAddressProfileDelegateIOSFactory(
                profile_));
  }

  MockAutofillSaveUpdateAddressProfileDelegateIOS& mock_delegate() {
    return *static_cast<MockAutofillSaveUpdateAddressProfileDelegateIOS*>(
        infobar_->delegate());
  }

 protected:
  SaveAddressProfileInfobarModalInteractionHandler handler_;
  MockAutofillSaveUpdateAddressProfileDelegateIOSFactory delegate_factory_;
  autofill::AutofillProfile profile_{
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode};
  std::unique_ptr<InfoBarIOS> infobar_;
};

TEST_F(SaveAddressProfileInfobarModalInteractionHandlerTest, MainAction) {
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(true));
  handler_.PerformMainAction(infobar_.get());
}

TEST_F(SaveAddressProfileInfobarModalInteractionHandlerTest, EditDeclined) {
  handler_.CancelModal(infobar_.get(), /*fromEditModal=*/YES);
  EXPECT_EQ(mock_delegate().user_decision(),
            autofill::AutofillClient::AddressPromptUserDecision::kEditDeclined);
}

TEST_F(SaveAddressProfileInfobarModalInteractionHandlerTest, Cancel) {
  handler_.CancelModal(infobar_.get(), /*fromEditModal=*/NO);
  EXPECT_EQ(mock_delegate().user_decision(),
            autofill::AutofillClient::AddressPromptUserDecision::kDeclined);
}

TEST_F(SaveAddressProfileInfobarModalInteractionHandlerTest, NoThanks) {
  EXPECT_CALL(mock_delegate(), Never());
  handler_.NoThanksWasPressed(infobar_.get());
}
