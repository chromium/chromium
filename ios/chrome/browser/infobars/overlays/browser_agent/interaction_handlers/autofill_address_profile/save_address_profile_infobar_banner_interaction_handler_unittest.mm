// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_banner_interaction_handler.h"

#include "base/guid.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_update_address_profile_delegate_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#include "ios/chrome/browser/infobars/test/mock_infobar_delegate.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for SaveAddressProfileInfobarBannerInteractionHandler.
class SaveAddressProfileInfobarBannerInteractionHandlerTest
    : public PlatformTest {
 public:
  SaveAddressProfileInfobarBannerInteractionHandlerTest()
      : delegate_factory_(),
        profile_(base::GenerateGUID(), "https://www.example.com/") {
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
  SaveAddressProfileInfobarBannerInteractionHandler handler_;
  MockAutofillSaveUpdateAddressProfileDelegateIOSFactory delegate_factory_;
  autofill::AutofillProfile profile_;
  std::unique_ptr<InfoBarIOS> infobar_;
};

// Tests that BannerVisibilityChanged() InfobarDismissed() on the mock delegate.
TEST_F(SaveAddressProfileInfobarBannerInteractionHandlerTest, Presentation) {
  EXPECT_CALL(mock_delegate(), InfoBarDismissed());
  handler_.BannerVisibilityChanged(infobar_.get(), false);
}
