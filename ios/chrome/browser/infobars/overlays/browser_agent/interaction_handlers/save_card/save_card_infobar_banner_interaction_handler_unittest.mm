// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_banner_interaction_handler.h"

#import "base/guid.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for SaveCardInfobarBannerInteractionHandler.
class SaveCardInfobarBannerInteractionHandlerTest : public PlatformTest {
 public:
  SaveCardInfobarBannerInteractionHandlerTest()
      : delegate_factory_(),
        card_(base::GenerateGUID(), "https://www.example.com/") {
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSaveCard,
        MockAutofillSaveCardInfoBarDelegateMobileFactory::
            CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(false,
                                                                   card_));
  }

  MockAutofillSaveCardInfoBarDelegateMobile& mock_delegate() {
    return *static_cast<MockAutofillSaveCardInfoBarDelegateMobile*>(
        infobar_->delegate());
  }

 protected:
  SaveCardInfobarBannerInteractionHandler handler_;
  MockAutofillSaveCardInfoBarDelegateMobileFactory delegate_factory_;
  autofill::CreditCard card_;
  std::unique_ptr<InfoBarIOS> infobar_;
};

TEST_F(SaveCardInfobarBannerInteractionHandlerTest, SaveCredentials) {
  std::u16string cardholder_name = base::SysNSStringToUTF16(@"test name");
  std::u16string expiration_date_month = base::SysNSStringToUTF16(@"06");
  std::u16string expiration_date_year = base::SysNSStringToUTF16(@"2023");
  EXPECT_CALL(mock_delegate(),
              UpdateAndAccept(cardholder_name, expiration_date_month,
                              expiration_date_year));
  handler_.SaveCredentials(infobar_.get(), cardholder_name,
                           expiration_date_month, expiration_date_year);
}

// Test that dismissing the banner does not call
// InfobarDelegate::InfobarDismissed(), which is a behavior for the other
// Infobars.
TEST_F(SaveCardInfobarBannerInteractionHandlerTest, DismissalNoDelegateCall) {
  EXPECT_CALL(mock_delegate(), InfoBarDismissed()).Times(0);
  handler_.BannerDismissedByUser(infobar_.get());
}
