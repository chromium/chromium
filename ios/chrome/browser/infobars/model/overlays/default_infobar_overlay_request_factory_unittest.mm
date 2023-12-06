// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/default_infobar_overlay_request_factory.h"

#import "base/feature_list.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/infobars/core/infobar.h"
#import "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/translate/core/browser/mock_translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_update_address_profile_delegate_ios.h"
#import "ios/chrome/browser/infobars/model/test/mock_infobar_delegate.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/model/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/test/mock_tailored_security_service_infobar_delegate.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileBannerRequestConfig;
using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;
using confirm_infobar_overlays::ConfirmBannerRequestConfig;
using infobars::InfoBar;
using infobars::InfoBarDelegate;
using safe_browsing::TailoredSecurityServiceMessageState;

using DefaultInfobarOverlayRequestFactoryTest = PlatformTest;

// Tests that the factory creates a save passwords infobar request.
TEST_F(DefaultInfobarOverlayRequestFactoryTest, SavePasswords) {
  GURL url("https://chromium.test");
  std::unique_ptr<InfoBarDelegate> delegate =
      MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username", @"password",
                                                       url);
  InfoBarIOS infobar(InfobarType::kInfobarTypePasswordSave,
                     std::move(delegate));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      DefaultInfobarOverlayRequestFactory(&infobar,
                                          InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request->GetConfig<DefaultInfobarOverlayRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      DefaultInfobarOverlayRequestFactory(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(modal_request->GetConfig<DefaultInfobarOverlayRequestConfig>());
}

// Tests that the factory creates an update passwords infobar request.
TEST_F(DefaultInfobarOverlayRequestFactoryTest, UpdatePasswords) {
  GURL url("https://chromium.test");
  std::unique_ptr<InfoBarDelegate> delegate =
      MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username", @"password",
                                                       url);
  InfoBarIOS infobar(InfobarType::kInfobarTypePasswordUpdate,
                     std::move(delegate));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      DefaultInfobarOverlayRequestFactory(&infobar,
                                          InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request->GetConfig<DefaultInfobarOverlayRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      DefaultInfobarOverlayRequestFactory(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(modal_request->GetConfig<DefaultInfobarOverlayRequestConfig>());
}

// Tests that the factory creates an confirm infobar request.
TEST_F(DefaultInfobarOverlayRequestFactoryTest, Confirm) {
  std::unique_ptr<MockInfobarDelegate> delegate =
      std::make_unique<MockInfobarDelegate>();
  InfoBarIOS infobar(InfobarType::kInfobarTypeConfirm, std::move(delegate));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      DefaultInfobarOverlayRequestFactory(&infobar,
                                          InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request->GetConfig<ConfirmBannerRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      DefaultInfobarOverlayRequestFactory(&infobar, InfobarOverlayType::kModal);
  EXPECT_FALSE(modal_request);
}

// Tests that the factory creates a save card request.
TEST_F(DefaultInfobarOverlayRequestFactoryTest, SaveCard) {
  autofill::CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                            "https://www.example.com/");

  InfoBarIOS infobar(
      InfobarType::kInfobarTypeSaveCard,
      MockAutofillSaveCardInfoBarDelegateMobileFactory::
          CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(false, card));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      DefaultInfobarOverlayRequestFactory(&infobar,
                                          InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request->GetConfig<DefaultInfobarOverlayRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      DefaultInfobarOverlayRequestFactory(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(modal_request->GetConfig<DefaultInfobarOverlayRequestConfig>());
}

// Tests that the factory creates a translate request.
TEST_F(DefaultInfobarOverlayRequestFactoryTest, Translate) {
  translate::testing::MockTranslateInfoBarDelegateFactory
      translate_delegate_factory("fr", "en");

  InfoBarIOS infobar(
      InfobarType::kInfobarTypeTranslate,
      translate_delegate_factory.CreateMockTranslateInfoBarDelegate(
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      DefaultInfobarOverlayRequestFactory(&infobar,
                                          InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request->GetConfig<DefaultInfobarOverlayRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      DefaultInfobarOverlayRequestFactory(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(modal_request->GetConfig<DefaultInfobarOverlayRequestConfig>());
}

// Tests that the factory creates a save address profile request.
TEST_F(DefaultInfobarOverlayRequestFactoryTest, SaveAddressProfile) {
  autofill::AutofillProfile profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  InfoBarIOS infobar(
      InfobarType::kInfobarTypeSaveAutofillAddressProfile,
      MockAutofillSaveUpdateAddressProfileDelegateIOSFactory::
          CreateMockAutofillSaveUpdateAddressProfileDelegateIOSFactory(
              profile));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      DefaultInfobarOverlayRequestFactory(&infobar,
                                          InfobarOverlayType::kBanner);
  EXPECT_TRUE(
      banner_request->GetConfig<SaveAddressProfileBannerRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      DefaultInfobarOverlayRequestFactory(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(modal_request->GetConfig<SaveAddressProfileModalRequestConfig>());
}

// Tests that the factory creates a tailored security service request.
TEST_F(DefaultInfobarOverlayRequestFactoryTest, TailoredSecurityService) {
  std::unique_ptr<InfoBarDelegate> delegate =
      safe_browsing::MockTailoredSecurityServiceInfobarDelegate::Create(
          /*message_state*/ TailoredSecurityServiceMessageState::
              kConsentedAndFlowEnabled,
          nullptr);
  InfoBarIOS infobar(InfobarType::kInfobarTypeTailoredSecurityService,
                     std::move(delegate));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      DefaultInfobarOverlayRequestFactory(&infobar,
                                          InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request->GetConfig<DefaultInfobarOverlayRequestConfig>());
}
