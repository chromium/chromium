// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_address_profile/save_address_profile_infobar_modal_interaction_handler.h"

#include <string>

#include "base/guid.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"
#include "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_address_profile_delegate_ios.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for SaveAddressProfileInfobarModalInteractionHandler.
class SaveAddressProfileInfobarModalInteractionHandlerTest
    : public PlatformTest {
 public:
  SaveAddressProfileInfobarModalInteractionHandlerTest()
      : delegate_factory_(),
        profile_(base::GenerateGUID(), "https://www.example.com/") {
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSaveAutofillAddressProfile,
        MockAutofillSaveAddressProfileDelegateIOSFactory::
            CreateMockAutofillSaveAddressProfileDelegateIOSFactory(profile_));
    handler_ =
        std::make_unique<SaveAddressProfileInfobarModalInteractionHandler>(
            &browser_);
  }

  MockAutofillSaveAddressProfileDelegateIOS& mock_delegate() {
    return *static_cast<MockAutofillSaveAddressProfileDelegateIOS*>(
        infobar_->delegate());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<SaveAddressProfileInfobarModalInteractionHandler> handler_;
  MockAutofillSaveAddressProfileDelegateIOSFactory delegate_factory_;
  autofill::AutofillProfile profile_;
  std::unique_ptr<InfoBarIOS> infobar_;
  TestBrowser browser_;
};

TEST_F(SaveAddressProfileInfobarModalInteractionHandlerTest, MainAction) {
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(true));
  handler_->PerformMainAction(infobar_.get());
}
