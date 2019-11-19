// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/autofill_payment_app.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_method_data.h"
#include "components/payments/core/payment_options.h"
#include "components/payments/core/payment_shipping_option.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "components/payments/core/web_payment_request.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::testing::_;

class MockTestPersonalDataManager : public autofill::TestPersonalDataManager {
 public:
  MockTestPersonalDataManager() : TestPersonalDataManager() {
    SetAutofillProfileEnabled(true);
    SetAutofillCreditCardEnabled(true);
    SetAutofillWalletImportEnabled(true);
  }

  MOCK_METHOD1(RecordUseOf, void(const autofill::AutofillDataModel&));
  MOCK_METHOD1(UpdateCreditCard, void(const autofill::CreditCard&));
  MOCK_METHOD1(UpdateServerCardMetadata, void(const autofill::CreditCard&));
  MOCK_METHOD1(UpdateProfile, void(const autofill::AutofillProfile&));
};

class MockPaymentsProfileComparator
    : public payments::PaymentsProfileComparator {
 public:
  MockPaymentsProfileComparator(const std::string& app_locale,
                                const payments::PaymentOptionsProvider& options)
      : PaymentsProfileComparator(app_locale, options) {}
  MOCK_METHOD1(Invalidate, void(const autofill::AutofillProfile&));
};

MATCHER_P(GuidMatches, guid, "") {
  return arg.guid() == guid;
}
}  // namespace

namespace payments {

class PaymentRequestTest : public PlatformTest {
 protected:
  PaymentRequestTest()
      : chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {
    test_personal_data_manager_.SetAutofillProfileEnabled(true);
    test_personal_data_manager_.SetAutofillCreditCardEnabled(true);
    test_personal_data_manager_.SetAutofillWalletImportEnabled(true);
  }

  // Returns PaymentDetails with one shipping option that's selected.
  PaymentDetails CreateDetailsWithShippingOption() {
    PaymentDetails details;
    std::vector<PaymentShippingOption> shipping_options;
    PaymentShippingOption option1;
    option1.id = "option:1";
    option1.selected = true;
    shipping_options.push_back(std::move(option1));
    details.shipping_options = std::move(shipping_options);

    return details;
  }

  PaymentOptions CreatePaymentOptions(bool request_payer_name,
                                      bool request_payer_phone,
                                      bool request_payer_email,
                                      bool request_shipping) {
    PaymentOptions options;
    options.request_payer_name = request_payer_name;
    options.request_payer_phone = request_payer_phone;
    options.request_payer_email = request_payer_email;
    options.request_shipping = request_shipping;
    return options;
  }

  base::test::TaskEnvironment task_environment_;

  autofill::TestPersonalDataManager test_personal_data_manager_;
  web::TestWebState web_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests that the CurrencyFormatter is constructed with the correct
// currency code and currency system.
TEST_F(PaymentRequestTest, CreatesCurrencyFormatterCorrectly) {
  WebPaymentRequest web_payment_request;

  web_payment_request.details.total = std::make_unique<PaymentItem>();
  web_payment_request.details.total->amount->currency = "USD";
  TestPaymentRequest payment_request1(web_payment_request,
                                      chrome_browser_state_.get(), &web_state_,
                                      &test_personal_data_manager_);
  ASSERT_EQ("en", payment_request1.GetApplicationLocale());
  CurrencyFormatter* currency_formatter =
      payment_request1.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("$55.00"), currency_formatter->Format("55.00"));
  EXPECT_EQ("USD", currency_formatter->formatted_currency_code());

  web_payment_request.details.total->amount->currency = "JPY";
  TestPaymentRequest payment_request2(web_payment_request,
                                      chrome_browser_state_.get(), &web_state_,
                                      &test_personal_data_manager_);
  ASSERT_EQ("en", payment_request2.GetApplicationLocale());
  currency_formatter = payment_request2.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("¥55"), currency_formatter->Format("55.00"));
  EXPECT_EQ("JPY", currency_formatter->formatted_currency_code());
}

// Tests that the accepted card networks are identified correctly.
TEST_F(PaymentRequestTest, AcceptedPaymentNetworks) {
  WebPaymentRequest web_payment_request;

  PaymentMethodData method_datum1;
  method_datum1.supported_method = "basic-card";
  method_datum1.supported_networks.push_back("visa");
  method_datum1.supported_networks.push_back("mastercard");
  web_payment_request.method_data.push_back(method_datum1);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  ASSERT_EQ(2U, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[1]);
}

// Test that parsing supported methods (with invalid values and duplicates)
// works as expected.
TEST_F(PaymentRequestTest, SupportedMethods) {
  WebPaymentRequest web_payment_request;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebPaymentsNativeApps);

  PaymentMethodData method_datum1;
  method_datum1.supported_method = "basic-card";
  method_datum1.supported_networks.push_back("visa");
  method_datum1.supported_networks.push_back("mastercard");
  PaymentMethodData method_datum2;
  method_datum2.supported_method = "mastercard";
  PaymentMethodData method_datum3;
  method_datum3.supported_method = "invalid";
  PaymentMethodData method_datum4;
  method_datum4.supported_method = "visa";
  PaymentMethodData method_datum5;
  method_datum5.supported_method = "https://bobpay.com";
  PaymentMethodData method_datum6;
  method_datum6.supported_method = "http://invalidpay.com";
  web_payment_request.method_data.push_back(method_datum1);
  web_payment_request.method_data.push_back(method_datum2);
  web_payment_request.method_data.push_back(method_datum3);
  web_payment_request.method_data.push_back(method_datum4);
  web_payment_request.method_data.push_back(method_datum5);
  web_payment_request.method_data.push_back(method_datum6);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  payment_request.ResetParsedPaymentMethodData();
  ASSERT_EQ(2U, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[1]);
  ASSERT_EQ(1U, payment_request.url_payment_method_identifiers().size());
  EXPECT_EQ(GURL("https://bobpay.com"),
            payment_request.url_payment_method_identifiers()[0]);
}

// Test that only specifying basic-card means that all are supported.
TEST_F(PaymentRequestTest, SupportedMethods_OnlyBasicCard) {
  WebPaymentRequest web_payment_request;

  PaymentMethodData method_datum1;
  method_datum1.supported_method = "basic-card";
  web_payment_request.method_data.push_back(method_datum1);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);

  // All of the basic card networks are supported.
  ASSERT_EQ(8U, payment_request.supported_card_networks().size());
  EXPECT_EQ("amex", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("diners", payment_request.supported_card_networks()[1]);
  EXPECT_EQ("discover", payment_request.supported_card_networks()[2]);
  EXPECT_EQ("jcb", payment_request.supported_card_networks()[3]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[4]);
  EXPECT_EQ("mir", payment_request.supported_card_networks()[5]);
  EXPECT_EQ("unionpay", payment_request.supported_card_networks()[6]);
  EXPECT_EQ("visa", payment_request.supported_card_networks()[7]);

  EXPECT_TRUE(payment_request.url_payment_method_identifiers().empty());
}

// Test that specifying basic-card with supported networks after specifying
// some methods
TEST_F(PaymentRequestTest, SupportedMethods_BasicCard_WithSupportedNetworks) {
  WebPaymentRequest web_payment_request;

  PaymentMethodData method_datum1;
  method_datum1.supported_method = "basic-card";
  method_datum1.supported_networks.push_back("visa");
  method_datum1.supported_networks.push_back("unionpay");
  web_payment_request.method_data.push_back(method_datum1);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);

  // Only the specified networks are supported.
  EXPECT_EQ(2u, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("unionpay", payment_request.supported_card_networks()[1]);
}

TEST_F(PaymentRequestTest, GooglePayCardsInBasicCard_Allowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kReturnGooglePayInBasicCard);

  WebPaymentRequest web_payment_request;
  PaymentMethodData method_datum;
  method_datum.supported_method = "basic-card";
  method_datum.supported_networks.push_back("mastercard");
  web_payment_request.method_data.push_back(method_datum);

  // Add a mastercard with billing address.
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test_personal_data_manager_.AddProfile(address);
  autofill::CreditCard credit_card = autofill::test::GetMaskedServerCard();
  credit_card.set_card_type(autofill::CreditCard::CardType::CARD_TYPE_CREDIT);
  credit_card.set_billing_address_id(address.guid());
  test_personal_data_manager_.AddServerCreditCard(credit_card);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);

  // The card is available in the payment request, and added to the
  // PersonalDataManager.
  EXPECT_EQ(1U, payment_request.payment_methods().size());
  // The card is expected to have been added to the PersonalDataManager.
  EXPECT_EQ(1U, test_personal_data_manager_.GetCreditCards().size());
}

TEST_F(PaymentRequestTest, GooglePayCardsInBasicCard_NotAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kReturnGooglePayInBasicCard);

  WebPaymentRequest web_payment_request;
  PaymentMethodData method_datum;
  method_datum.supported_method = "basic-card";
  method_datum.supported_networks.push_back("mastercard");
  web_payment_request.method_data.push_back(method_datum);

  // Add a mastercard with billing address.
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test_personal_data_manager_.AddProfile(address);
  autofill::CreditCard credit_card = autofill::test::GetMaskedServerCard();
  credit_card.set_card_type(autofill::CreditCard::CardType::CARD_TYPE_CREDIT);
  credit_card.set_billing_address_id(address.guid());
  test_personal_data_manager_.AddServerCreditCard(credit_card);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);

  // The card is not available in the payment request, but added to the
  // PersonalDataManager.
  EXPECT_TRUE(payment_request.payment_methods().empty());
  // The card is expected to have been added to the PersonalDataManager.
  EXPECT_EQ(1U, test_personal_data_manager_.GetCreditCards().size());
}

// Tests that an autofill payment instrumnt e.g., credit cards can be added
// to the list of available payment methods.
TEST_F(PaymentRequestTest, CreateAndAddAutofillPaymentInstrument) {
  WebPaymentRequest web_payment_request;
  PaymentMethodData method_datum;
  method_datum.supported_method = "basic-card";
  method_datum.supported_networks.push_back("visa");
  web_payment_request.method_data.push_back(method_datum);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(0U, payment_request.payment_methods().size());

  autofill::CreditCard credit_card_1 = autofill::test::GetCreditCard();
  AutofillPaymentApp* added_credit_card_1 =
      payment_request.CreateAndAddAutofillPaymentInstrument(credit_card_1);
  EXPECT_EQ(credit_card_1, *added_credit_card_1->credit_card());

  EXPECT_EQ(1U, payment_request.payment_methods().size());
  // The card is expected to have been added to the PersonalDataManager.
  EXPECT_EQ(1U, test_personal_data_manager_.GetCreditCards().size());
}

// Tests that an autofill payment instrumnt e.g., credit cards can be added
// to the list of available payment methods in incognito mode.
TEST_F(PaymentRequestTest, CreateAndAddAutofillPaymentInstrumentIncognito) {
  WebPaymentRequest web_payment_request;
  PaymentMethodData method_datum;
  method_datum.supported_method = "basic-card";
  method_datum.supported_networks.push_back("visa");
  web_payment_request.method_data.push_back(method_datum);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(0U, payment_request.payment_methods().size());

  payment_request.set_is_incognito(true);

  autofill::CreditCard credit_card_1 = autofill::test::GetCreditCard();
  AutofillPaymentApp* added_credit_card_1 =
      payment_request.CreateAndAddAutofillPaymentInstrument(credit_card_1);
  EXPECT_EQ(credit_card_1, *added_credit_card_1->credit_card());

  EXPECT_EQ(1U, payment_request.payment_methods().size());
  // The card should not get added to the PersonalDataManager.
  EXPECT_EQ(0U, test_personal_data_manager_.GetCreditCards().size());
}

// Tests updating local and server autofill payment instruments.
TEST_F(PaymentRequestTest, UpdateAutofillPaymentInstrument) {
  WebPaymentRequest web_payment_request;
  PaymentMethodData method_datum;
  method_datum.supported_method = "basic-card";
  method_datum.supported_networks.push_back("visa");
  method_datum.supported_networks.push_back("amex");
  web_payment_request.method_data.push_back(method_datum);

  MockTestPersonalDataManager personal_data_manager;
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);

  // Credit card should get updated in Personal Data Manager.
  EXPECT_CALL(personal_data_manager, UpdateCreditCard(_)).Times(1);

  autofill::CreditCard credit_card_1 = autofill::test::GetCreditCard();
  payment_request.UpdateAutofillPaymentInstrument(credit_card_1);

  // Only credit card's meta data should get updated in PersonalDataManager.
  EXPECT_CALL(personal_data_manager, UpdateServerCardMetadata(_)).Times(1);

  autofill::CreditCard credit_card_2 =
      autofill::test::GetMaskedServerCardAmex();
  payment_request.UpdateAutofillPaymentInstrument(credit_card_2);
}

// Tests updating local and server autofill payment instruments in incognito
// mode.
TEST_F(PaymentRequestTest, UpdateAutofillPaymentInstrumentIncognito) {
  WebPaymentRequest web_payment_request;
  PaymentMethodData method_datum;
  method_datum.supported_method = "basic-card";
  method_datum.supported_networks.push_back("visa");
  method_datum.supported_networks.push_back("amex");
  web_payment_request.method_data.push_back(method_datum);

  MockTestPersonalDataManager personal_data_manager;
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);

  payment_request.set_is_incognito(true);

  // Credit card should not get updated in Personal Data Manager.
  EXPECT_CALL(personal_data_manager, UpdateCreditCard(_)).Times(0);

  autofill::CreditCard credit_card_1 = autofill::test::GetCreditCard();
  payment_request.UpdateAutofillPaymentInstrument(credit_card_1);

  // Only credit card's meta data should not get updated in PersonalDataManager.
  EXPECT_CALL(personal_data_manager, UpdateServerCardMetadata(_)).Times(0);

  autofill::CreditCard credit_card_2 =
      autofill::test::GetMaskedServerCardAmex();
  payment_request.UpdateAutofillPaymentInstrument(credit_card_2);
}

// Tests that a profile can be added to the list of available profiles.
TEST_F(PaymentRequestTest, AddAutofillProfile) {
  WebPaymentRequest web_payment_request;
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(0U, payment_request.shipping_profiles().size());
  EXPECT_EQ(0U, payment_request.contact_profiles().size());

  autofill::AutofillProfile profile_1 = autofill::test::GetFullProfile();
  autofill::AutofillProfile* added_profile_1 =
      payment_request.AddAutofillProfile(profile_1);
  EXPECT_EQ(profile_1, *added_profile_1);

  EXPECT_EQ(1U, payment_request.shipping_profiles().size());
  EXPECT_EQ(1U, payment_request.contact_profiles().size());
  // The autofill profile should have been added to the PersonalDataManager.
  EXPECT_EQ(1U, test_personal_data_manager_.GetProfiles().size());
}

// Tests that a profile can be added to the list of available profiles in
// incognito mode.
TEST_F(PaymentRequestTest, AddAutofillProfileIncognito) {
  WebPaymentRequest web_payment_request;
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(0U, payment_request.shipping_profiles().size());
  EXPECT_EQ(0U, payment_request.contact_profiles().size());

  payment_request.set_is_incognito(true);

  autofill::AutofillProfile profile_1 = autofill::test::GetFullProfile();
  autofill::AutofillProfile* added_profile_1 =
      payment_request.AddAutofillProfile(profile_1);
  EXPECT_EQ(profile_1, *added_profile_1);

  EXPECT_EQ(1U, payment_request.shipping_profiles().size());
  EXPECT_EQ(1U, payment_request.contact_profiles().size());
  // The autofill profile should not get added to the PersonalDataManager.
  EXPECT_EQ(0U, test_personal_data_manager_.GetProfiles().size());
}

// Tests updating an autofill profile.
TEST_F(PaymentRequestTest, UpdateAutofillProfile) {
  WebPaymentRequest web_payment_request;
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  MockTestPersonalDataManager personal_data_manager;
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);

  MockPaymentsProfileComparator profile_comparator(
      payment_request.GetApplicationLocale(), payment_request);
  payment_request.SetProfileComparator(&profile_comparator);

  // Profile should get updated in PersonalDataManager.
  EXPECT_CALL(personal_data_manager, UpdateProfile(_)).Times(1);

  EXPECT_CALL(profile_comparator, Invalidate(_)).Times(1);

  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  payment_request.UpdateAutofillProfile(profile);
}

// Tests updating an autofill profile in incognito mode.
TEST_F(PaymentRequestTest, UpdateAutofillProfileIncognito) {
  WebPaymentRequest web_payment_request;
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  MockTestPersonalDataManager personal_data_manager;
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);

  payment_request.set_is_incognito(true);

  MockPaymentsProfileComparator profile_comparator(
      payment_request.GetApplicationLocale(), payment_request);
  payment_request.SetProfileComparator(&profile_comparator);

  // Profile should not get updated in PersonalDataManager.
  EXPECT_CALL(personal_data_manager, UpdateProfile(_)).Times(0);

  EXPECT_CALL(profile_comparator, Invalidate(_)).Times(1);

  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  payment_request.UpdateAutofillProfile(profile);
}

// Test that parsing shipping options works as expected.
TEST_F(PaymentRequestTest, SelectedShippingOptions) {
  WebPaymentRequest web_payment_request;

  PaymentDetails details;
  details.total = std::make_unique<PaymentItem>();
  std::vector<PaymentShippingOption> shipping_options;
  PaymentShippingOption option1;
  option1.id = "option:1";
  option1.selected = false;
  shipping_options.push_back(std::move(option1));
  PaymentShippingOption option2;
  option2.id = "option:2";
  option2.selected = true;
  shipping_options.push_back(std::move(option2));
  PaymentShippingOption option3;
  option3.id = "option:3";
  option3.selected = true;
  shipping_options.push_back(std::move(option3));
  details.shipping_options = std::move(shipping_options);
  web_payment_request.details = std::move(details);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  // The last one marked "selected" should be selected.
  EXPECT_EQ("option:3", payment_request.selected_shipping_option()->id);

  // Simulate an update that no longer has any shipping options. There is no
  // longer a selected shipping option.
  PaymentDetails new_details;
  payment_request.UpdatePaymentDetails(std::move(new_details));
  EXPECT_EQ(nullptr, payment_request.selected_shipping_option());
}

// Tests that updating the payment details updates the total amount.
TEST_F(PaymentRequestTest, UpdatePaymentDetailsNewTotal) {
  WebPaymentRequest web_payment_request;

  PaymentDetails details;
  details.total = std::make_unique<PaymentItem>();
  details.total->amount->value = "10.00";
  details.total->amount->currency = "USD";
  web_payment_request.details = std::move(details);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);

  // Simulate an update with a new total amount.
  PaymentDetails new_details;
  new_details.total = std::make_unique<PaymentItem>();
  new_details.total->amount->value = "20.00";
  new_details.total->amount->currency = "CAD";
  payment_request.UpdatePaymentDetails(std::move(new_details));
  EXPECT_EQ("20.00", payment_request.payment_details().total->amount->value);
  EXPECT_EQ("CAD", payment_request.payment_details().total->amount->currency);
}

// Tests that updating the payment details with a PaymentDetails instance that
// is missing the total amount, maintains the old total amount.
TEST_F(PaymentRequestTest, UpdatePaymentDetailsNoTotal) {
  WebPaymentRequest web_payment_request;

  PaymentDetails details;
  details.total = std::make_unique<PaymentItem>();
  details.total->amount->value = "10.00";
  details.total->amount->currency = "USD";
  web_payment_request.details = std::move(details);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);

  // Simulate an update with the total amount missing.
  PaymentDetails new_details;
  payment_request.UpdatePaymentDetails(std::move(new_details));
  EXPECT_EQ("10.00", payment_request.payment_details().total->amount->value);
  EXPECT_EQ("USD", payment_request.payment_details().total->amount->currency);
}

// Test that loading profiles when none are available works as expected.
TEST_F(PaymentRequestTest, SelectedProfiles_NoProfiles) {
  WebPaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // No profiles are selected because none are available!
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(nullptr, payment_request.selected_shipping_profile());
  EXPECT_EQ(nullptr, payment_request.selected_contact_profile());
}

// Test that loading complete shipping and contact profiles works as expected.
TEST_F(PaymentRequestTest, SelectedProfiles_Complete) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.set_use_count(5U);
  test_personal_data_manager_.AddProfile(address);
  autofill::AutofillProfile address2 = autofill::test::GetFullProfile2();
  address2.set_use_count(15U);
  test_personal_data_manager_.AddProfile(address2);

  WebPaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // address2 is selected because it has the most use count (Frecency model).
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(address2.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(address2.guid(),
            payment_request.selected_contact_profile()->guid());
}

// Test that loading complete shipping and contact profiles, when there are no
// shipping options available, works as expected.
TEST_F(PaymentRequestTest, SelectedProfiles_Complete_NoShippingOption) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.set_use_count(5U);
  test_personal_data_manager_.AddProfile(address);

  WebPaymentRequest web_payment_request;
  // No shipping options.
  web_payment_request.details = PaymentDetails();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // No shipping profile is selected because the merchant has not selected a
  // shipping option. However there is a suitable contact profile.
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(nullptr, payment_request.selected_shipping_profile());
  EXPECT_EQ(address.guid(), payment_request.selected_contact_profile()->guid());
}

// Test that loading incomplete shipping and contact profiles works as expected.
TEST_F(PaymentRequestTest, SelectedProfiles_Incomplete) {
  // Add a profile with no phone (incomplete).
  autofill::AutofillProfile address1 = autofill::test::GetFullProfile();
  address1.SetInfo(autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                   base::string16(), "en-US");
  address1.set_use_count(5U);
  test_personal_data_manager_.AddProfile(address1);
  // Add a complete profile, with fewer use counts.
  autofill::AutofillProfile address2 = autofill::test::GetFullProfile2();
  address2.set_use_count(3U);
  test_personal_data_manager_.AddProfile(address2);

  WebPaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // Even though address1 has more use counts, address2 is selected because it
  // is complete.
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(address2.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(address2.guid(),
            payment_request.selected_contact_profile()->guid());
}

// Test that loading incomplete contact profiles works as expected when the
// merchant is not interested in the missing field. Test that the most complete
// shipping profile is selected.
TEST_F(PaymentRequestTest,
       SelectedProfiles_IncompleteContact_NoRequestPayerPhone) {
  // Add a profile with no phone (incomplete).
  autofill::AutofillProfile address1 = autofill::test::GetFullProfile();
  address1.SetInfo(autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                   base::string16(), "en-US");
  address1.set_use_count(5U);
  test_personal_data_manager_.AddProfile(address1);
  // Add a complete profile, with fewer use counts.
  autofill::AutofillProfile address2 = autofill::test::GetFullProfile();
  address2.set_use_count(3U);
  test_personal_data_manager_.AddProfile(address2);

  WebPaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  // The merchant doesn't care about the phone number.
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/false,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // address1 has more use counts, and even though it has no phone number, it's
  // still selected as the contact profile because merchant doesn't require
  // phone. address2 is selected as the shipping profile because it's the most
  // complete for shipping.
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(address2.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(address1.guid(),
            payment_request.selected_contact_profile()->guid());
}

// Test that loading payment methods when none are available works as expected.
TEST_F(PaymentRequestTest, SelectedPaymentMethod_NoPaymentMethods) {
  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  // No payment methods are selected because none are available!
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(nullptr, payment_request.selected_payment_method());
}

// Test that loading expired credit cards works as expected.
TEST_F(PaymentRequestTest, SelectedPaymentMethod_ExpiredCard) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  test_personal_data_manager_.AddProfile(billing_address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.SetExpirationYear(2016);  // Expired.
  credit_card.set_billing_address_id(billing_address.guid());
  test_personal_data_manager_.AddCreditCard(credit_card);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  // credit_card is selected because expired cards are valid for payment.
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  EXPECT_EQ(payment_request.selected_payment_method()->type(),
            PaymentApp::Type::AUTOFILL);
  AutofillPaymentApp* payment_instrument = static_cast<AutofillPaymentApp*>(
      payment_request.selected_payment_method());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());
}

// Test that loading complete payment methods works as expected.
TEST_F(PaymentRequestTest, SelectedPaymentMethod_Complete) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  test_personal_data_manager_.AddProfile(billing_address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_use_count(5U);
  credit_card.set_billing_address_id(billing_address.guid());
  test_personal_data_manager_.AddCreditCard(credit_card);
  autofill::CreditCard credit_card2 = autofill::test::GetCreditCard2();
  credit_card2.set_use_count(15U);
  credit_card2.set_billing_address_id(billing_address.guid());
  test_personal_data_manager_.AddCreditCard(credit_card2);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  // credit_card2 is selected because it has the most use count (Frecency
  // model).
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  AutofillPaymentApp* payment_instrument = static_cast<AutofillPaymentApp*>(
      payment_request.selected_payment_method());
  EXPECT_EQ(credit_card2.guid(), payment_instrument->credit_card()->guid());
}

// Test that loading incomplete payment methods works as expected.
TEST_F(PaymentRequestTest, SelectedPaymentMethod_Incomplete) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  test_personal_data_manager_.AddProfile(billing_address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_use_count(5U);
  credit_card.set_billing_address_id(billing_address.guid());
  test_personal_data_manager_.AddCreditCard(credit_card);
  autofill::CreditCard credit_card2 = autofill::test::GetCreditCard2();
  credit_card2.set_use_count(15U);
  test_personal_data_manager_.AddCreditCard(credit_card2);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  // Even though credit_card2 has more use counts, credit_card is selected
  // because it is complete.
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  AutofillPaymentApp* payment_instrument = static_cast<AutofillPaymentApp*>(
      payment_request.selected_payment_method());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());
}

// Test that the use counts of the data models are updated as expected when
// different autofill profiles are used as the shipping address and the contact
// info.
TEST_F(PaymentRequestTest, RecordUseStats_RequestShippingAndContactInfo) {
  MockTestPersonalDataManager personal_data_manager;
  // Add a profile that is incomplete for a contact info, but is used more
  // frequently so is selected as the default shipping address.
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetInfo(autofill::AutofillType(autofill::EMAIL_ADDRESS),
                  base::string16(), "en-US");
  address.set_use_count(10U);
  personal_data_manager.AddProfile(address);
  autofill::AutofillProfile contact_info = autofill::test::GetFullProfile2();
  contact_info.set_use_count(5U);
  personal_data_manager.AddProfile(contact_info);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_billing_address_id(address.guid());
  personal_data_manager.AddCreditCard(credit_card);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentApp* payment_instrument = static_cast<AutofillPaymentApp*>(
      payment_request.selected_payment_method());
  EXPECT_EQ(address.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(contact_info.guid(),
            payment_request.selected_contact_profile()->guid());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());

  EXPECT_CALL(personal_data_manager, RecordUseOf(GuidMatches(address.guid())))
      .Times(1);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(contact_info.guid())))
      .Times(1);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(credit_card.guid())))
      .Times(1);

  payment_request.RecordUseStats();
}

// Test that the use counts of the data models are updated as expected when the
// same autofill profile is used as the shipping address and the contact info.
TEST_F(PaymentRequestTest, RecordUseStats_SameShippingAndContactInfoProfile) {
  MockTestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddProfile(address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_billing_address_id(address.guid());
  personal_data_manager.AddCreditCard(credit_card);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentApp* payment_instrument = static_cast<AutofillPaymentApp*>(
      payment_request.selected_payment_method());
  EXPECT_EQ(address.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(address.guid(), payment_request.selected_contact_profile()->guid());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());

  // Even though |address| is used for contact info, shipping address, and
  // credit_card's billing address, the stats should be updated only once.
  EXPECT_CALL(personal_data_manager, RecordUseOf(GuidMatches(address.guid())))
      .Times(1);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(credit_card.guid())))
      .Times(1);

  payment_request.RecordUseStats();
}

// Test that the use counts of the data models are updated as expected when no
// contact information is requested.
TEST_F(PaymentRequestTest, RecordUseStats_RequestShippingOnly) {
  MockTestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddProfile(address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_billing_address_id(address.guid());
  personal_data_manager.AddCreditCard(credit_card);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  web_payment_request.options.request_payer_name = false;
  web_payment_request.options.request_payer_email = false;
  web_payment_request.options.request_payer_phone = false;

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentApp* payment_instrument = static_cast<AutofillPaymentApp*>(
      payment_request.selected_payment_method());
  EXPECT_EQ(address.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(nullptr, payment_request.selected_contact_profile());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());

  EXPECT_CALL(personal_data_manager, RecordUseOf(GuidMatches(address.guid())))
      .Times(1);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(credit_card.guid())))
      .Times(1);

  payment_request.RecordUseStats();
}

// Test that the use counts of the data models are updated as expected when no
// shipping information is requested.
TEST_F(PaymentRequestTest, RecordUseStats_RequestContactInfoOnly) {
  MockTestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddProfile(address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_billing_address_id(address.guid());
  personal_data_manager.AddCreditCard(credit_card);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  web_payment_request.options.request_shipping = false;

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentApp* payment_instrument = static_cast<AutofillPaymentApp*>(
      payment_request.selected_payment_method());
  EXPECT_EQ(nullptr, payment_request.selected_shipping_profile());
  EXPECT_EQ(address.guid(), payment_request.selected_contact_profile()->guid());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());

  EXPECT_CALL(personal_data_manager, RecordUseOf(GuidMatches(address.guid())))
      .Times(1);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(credit_card.guid())))
      .Times(1);

  payment_request.RecordUseStats();
}

// Test that the use counts of the data models are updated as expected when no
// shipping or contact information is requested.
TEST_F(PaymentRequestTest, RecordUseStats_NoShippingOrContactInfoRequested) {
  MockTestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddProfile(address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_billing_address_id(address.guid());
  personal_data_manager.AddCreditCard(credit_card);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  web_payment_request.options.request_shipping = false;
  web_payment_request.options.request_payer_name = false;
  web_payment_request.options.request_payer_email = false;
  web_payment_request.options.request_payer_phone = false;

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentApp* payment_instrument = static_cast<AutofillPaymentApp*>(
      payment_request.selected_payment_method());
  EXPECT_EQ(nullptr, payment_request.selected_shipping_profile());
  EXPECT_EQ(nullptr, payment_request.selected_contact_profile());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());

  EXPECT_CALL(personal_data_manager, RecordUseOf(GuidMatches(address.guid())))
      .Times(0);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(credit_card.guid())))
      .Times(1);

  payment_request.RecordUseStats();
}

// Tests that the modifier should not get applied when the card network is not
// supported.
TEST_F(PaymentRequestTest, PaymentDetailsModifier_BasicCard_NetworkMismatch) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test_personal_data_manager_.AddProfile(address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();  // Visa.
  test_personal_data_manager_.AddCreditCard(credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  PaymentDetailsModifier modifier;
  modifier.method_data.supported_method = "basic-card";
  modifier.method_data.supported_networks.push_back("amex");
  modifier.total = std::make_unique<payments::PaymentItem>();
  modifier.total->label = "Discounted Total";
  modifier.total->amount->value = "0.99";
  modifier.total->amount->currency = "USD";
  payments::PaymentItem additional_display_item;
  additional_display_item.label = "Amex discount";
  additional_display_item.amount->value = "-0.01";
  additional_display_item.amount->currency = "USD";
  modifier.additional_display_items.push_back(additional_display_item);
  web_payment_request.details.modifiers.push_back(modifier);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  AutofillPaymentApp* selected_payment_method =
      static_cast<AutofillPaymentApp*>(
          payment_request.selected_payment_method());
  EXPECT_EQ("Total", payment_request.GetTotal(selected_payment_method).label);
  EXPECT_EQ("1.00",
            payment_request.GetTotal(selected_payment_method).amount->value);
  ASSERT_EQ(1U,
            payment_request.GetDisplayItems(selected_payment_method).size());
}

// Tests that the modifier should get applied when the card network is a match.
TEST_F(PaymentRequestTest, PaymentDetailsModifier_BasicCard_NetworkMatch) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test_personal_data_manager_.AddProfile(address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard2();  // Amex.
  credit_card.set_billing_address_id(address.guid());
  test_personal_data_manager_.AddCreditCard(credit_card);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  PaymentDetailsModifier modifier;
  modifier.method_data.supported_method = "basic-card";
  modifier.method_data.supported_networks.push_back("amex");
  modifier.total = std::make_unique<payments::PaymentItem>();
  modifier.total->label = "Discounted Total";
  modifier.total->amount->value = "0.99";
  modifier.total->amount->currency = "USD";
  payments::PaymentItem additional_display_item;
  additional_display_item.label = "Amex discount";
  additional_display_item.amount->value = "-0.01";
  additional_display_item.amount->currency = "USD";
  modifier.additional_display_items.push_back(additional_display_item);
  web_payment_request.details.modifiers.push_back(modifier);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  AutofillPaymentApp* selected_payment_method =
      static_cast<AutofillPaymentApp*>(
          payment_request.selected_payment_method());
  EXPECT_EQ("Discounted Total",
            payment_request.GetTotal(selected_payment_method).label);
  EXPECT_EQ("0.99",
            payment_request.GetTotal(selected_payment_method).amount->value);
  ASSERT_EQ(2U,
            payment_request.GetDisplayItems(selected_payment_method).size());
  EXPECT_EQ("Subtotal",
            payment_request.GetDisplayItems(selected_payment_method)[0].label);
  EXPECT_EQ("1.00", payment_request.GetDisplayItems(selected_payment_method)[0]
                        .amount->value);
  EXPECT_EQ("Amex discount",
            payment_request.GetDisplayItems(selected_payment_method)[1].label);
  EXPECT_EQ("-0.01", payment_request.GetDisplayItems(selected_payment_method)[1]
                         .amount->value);
}

// Tests that the modifier should not get applied when the card type is not
// supported.
TEST_F(PaymentRequestTest, PaymentDetailsModifier_BasicCard_TypeMismatch) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test_personal_data_manager_.AddProfile(address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard2();  // Amex.
  test_personal_data_manager_.AddCreditCard(credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  PaymentDetailsModifier modifier;
  modifier.method_data.supported_method = "basic-card";
  modifier.method_data.supported_networks.push_back("amex");
  modifier.method_data.supported_types.insert(
      autofill::CreditCard::CARD_TYPE_CREDIT);
  modifier.total = std::make_unique<payments::PaymentItem>();
  modifier.total->label = "Discounted Total";
  modifier.total->amount->value = "0.99";
  modifier.total->amount->currency = "USD";
  payments::PaymentItem additional_display_item;
  additional_display_item.label = "Amex discount";
  additional_display_item.amount->value = "-0.01";
  additional_display_item.amount->currency = "USD";
  modifier.additional_display_items.push_back(additional_display_item);
  web_payment_request.details.modifiers.push_back(modifier);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  AutofillPaymentApp* selected_payment_method =
      static_cast<AutofillPaymentApp*>(
          payment_request.selected_payment_method());
  EXPECT_EQ("Total", payment_request.GetTotal(selected_payment_method).label);
  EXPECT_EQ("1.00",
            payment_request.GetTotal(selected_payment_method).amount->value);
  ASSERT_EQ(1U,
            payment_request.GetDisplayItems(selected_payment_method).size());
}

// Tests that the modifier should get applied when the card network and the type
// are both a match.
TEST_F(PaymentRequestTest,
       PaymentDetailsModifier_BasicCard_NetworkAndTypeMatch) {
  // Only Google Pay cards have a type.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kReturnGooglePayInBasicCard);

  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test_personal_data_manager_.AddProfile(address);
  autofill::CreditCard credit_card = autofill::test::GetMaskedServerCardAmex();
  credit_card.set_card_type(autofill::CreditCard::CardType::CARD_TYPE_CREDIT);
  credit_card.set_billing_address_id(address.guid());
  test_personal_data_manager_.AddServerCreditCard(credit_card);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  PaymentDetailsModifier modifier;
  modifier.method_data.supported_method = "basic-card";
  modifier.method_data.supported_networks.push_back("amex");
  modifier.method_data.supported_types.insert(
      autofill::CreditCard::CARD_TYPE_CREDIT);
  modifier.total = std::make_unique<payments::PaymentItem>();
  modifier.total->label = "Discounted Total";
  modifier.total->amount->value = "0.99";
  modifier.total->amount->currency = "USD";
  payments::PaymentItem additional_display_item;
  additional_display_item.label = "Amex discount";
  additional_display_item.amount->value = "-0.01";
  additional_display_item.amount->currency = "USD";
  modifier.additional_display_items.push_back(additional_display_item);
  web_payment_request.details.modifiers.push_back(modifier);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);
  AutofillPaymentApp* selected_payment_method =
      static_cast<AutofillPaymentApp*>(
          payment_request.selected_payment_method());
  EXPECT_EQ("Discounted Total",
            payment_request.GetTotal(selected_payment_method).label);
  EXPECT_EQ("0.99",
            payment_request.GetTotal(selected_payment_method).amount->value);
  ASSERT_EQ(2U,
            payment_request.GetDisplayItems(selected_payment_method).size());
  EXPECT_EQ("Subtotal",
            payment_request.GetDisplayItems(selected_payment_method)[0].label);
  EXPECT_EQ("1.00", payment_request.GetDisplayItems(selected_payment_method)[0]
                        .amount->value);
  EXPECT_EQ("Amex discount",
            payment_request.GetDisplayItems(selected_payment_method)[1].label);
  EXPECT_EQ("-0.01", payment_request.GetDisplayItems(selected_payment_method)[1]
                         .amount->value);
}

// Tests that payment_request_util::RequestContactInfo returns true if payer's
// name, phone number, or email address are requested and false otherwise.
TEST_F(PaymentRequestTest, RequestContactInfo) {
  payments::WebPaymentRequest web_payment_request;

  web_payment_request.options.request_payer_name = true;
  web_payment_request.options.request_payer_phone = true;
  web_payment_request.options.request_payer_email = true;
  payments::TestPaymentRequest payment_request1(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_TRUE(payment_request1.RequestContactInfo());

  web_payment_request.options.request_payer_name = false;
  payments::TestPaymentRequest payment_request2(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_TRUE(payment_request2.RequestContactInfo());

  web_payment_request.options.request_payer_phone = false;
  payments::TestPaymentRequest payment_request3(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_TRUE(payment_request3.RequestContactInfo());

  web_payment_request.options.request_payer_email = false;
  payments::TestPaymentRequest payment_request4(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_FALSE(payment_request4.RequestContactInfo());
}

// Tests the return value of payment_request_util::CanPay.
TEST_F(PaymentRequestTest, CanPay) {
  payments::WebPaymentRequest web_payment_request;
  payments::PaymentMethodData method_datum;
  method_datum.supported_method = "basic-card";
  method_datum.supported_networks.push_back("visa");
  web_payment_request.method_data.push_back(method_datum);

  // No selected payment method.
  payments::TestPaymentRequest payment_request1(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_FALSE(payment_request1.IsAbleToPay());

  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  // Make the profile incomplete by removing the name and the phone number so
  // that it is not selected as the default shipping address or contact info.
  profile.SetInfo(autofill::AutofillType(autofill::NAME_FULL), base::string16(),
                  "en-US");
  profile.SetInfo(autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                  base::string16(), "en-US");
  test_personal_data_manager_.AddProfile(profile);
  autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  card.set_billing_address_id(profile.guid());
  test_personal_data_manager_.AddCreditCard(card);

  // Has a selected payment method.
  payments::TestPaymentRequest payment_request2(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_TRUE(payment_request2.IsAbleToPay());

  // No selected contact info.
  web_payment_request.options.request_payer_phone = true;
  payments::TestPaymentRequest payment_request3(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_FALSE(payment_request3.IsAbleToPay());

  test_personal_data_manager_.GetProfiles()[0]->SetInfo(
      autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
      base::ASCIIToUTF16("16502111111"), "en-US");

  // Has a selected contact info.
  payments::TestPaymentRequest payment_request4(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_TRUE(payment_request4.IsAbleToPay());

  // No selected shipping address.
  web_payment_request.options.request_shipping = true;
  payments::TestPaymentRequest payment_request5(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_FALSE(payment_request5.IsAbleToPay());

  test_personal_data_manager_.GetProfiles()[0]->SetInfo(
      autofill::AutofillType(autofill::NAME_FULL),
      base::ASCIIToUTF16("John Doe"), "en-US");

  // Has a selected shipping address, but no selected shipping option.
  payments::TestPaymentRequest payment_request6(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_FALSE(payment_request6.IsAbleToPay());

  std::vector<payments::PaymentShippingOption> shipping_options;
  payments::PaymentShippingOption option;
  option.id = "1";
  option.selected = true;
  shipping_options.push_back(std::move(option));
  web_payment_request.details.shipping_options = std::move(shipping_options);

  // Has a selected shipping address and a selected shipping option.
  payments::TestPaymentRequest payment_request7(
      web_payment_request, chrome_browser_state_.get(), &web_state_,
      &test_personal_data_manager_);
  EXPECT_TRUE(payment_request7.IsAbleToPay());
}

}  // namespace payments
