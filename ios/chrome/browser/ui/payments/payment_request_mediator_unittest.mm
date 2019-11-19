// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_mediator.h"

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/core/payment_app.h"
#include "components/payments/core/payment_prefs.h"
#include "components/payments/core/payment_shipping_option.h"
#include "components/payments/core/strings_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/test_sync_service.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#include "ios/chrome/browser/payments/payment_request_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_delegate_fake.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payments::GetShippingOptionSectionString;
using ::payment_request_util::GetEmailLabelFromAutofillProfile;
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetShippingAddressLabelFromAutofillProfile;

std::unique_ptr<KeyedService> CreateTestSyncService(
    web::BrowserState* context) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<KeyedService> BuildMockSyncSetupService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SyncSetupServiceMock>(
      ProfileSyncServiceFactory::GetForBrowserState(browser_state));
}
}  // namespace

class PaymentRequestMediatorTest : public PaymentRequestUnitTestBase,
                                   public PlatformTest {
 protected:
  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::TestingFactories factories;
    factories.emplace_back(ProfileSyncServiceFactory::GetInstance(),
                           base::BindRepeating(&CreateTestSyncService));
    factories.emplace_back(SyncSetupServiceFactory::GetInstance(),
                           base::BindRepeating(&BuildMockSyncSetupService));
    factories.emplace_back(AuthenticationServiceFactory::GetInstance(),
                           AuthenticationServiceFactory::GetDefaultFactory());
    DoSetUp(std::move(factories));

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state(), std::make_unique<AuthenticationServiceDelegateFake>());

    ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
        ->AddIdentities(@[ @"username" ]);

    autofill::AutofillProfile profile = autofill::test::GetFullProfile();
    autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
    card.set_billing_address_id(profile.guid());
    AddAutofillProfile(std::move(profile));
    AddCreditCard(std::move(card));

    CreateTestPaymentRequest();

    mediator_ = [[PaymentRequestMediator alloc]
        initWithPaymentRequest:payment_request()];
  }

  // PlatformTest:
  void TearDown() override {
    DoTearDown();
    PlatformTest::TearDown();
  }

  PaymentRequestMediator* mediator() { return mediator_; }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForBrowserState(browser_state());
  }

  ChromeIdentity* fake_identity() {
    return [ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
                ->GetAllIdentities() firstObject];
  }

 private:
  PaymentRequestMediator* mediator_;
};

// Tests whether payment can be completed when expected.
TEST_F(PaymentRequestMediatorTest, TestCanPay) {
  EXPECT_TRUE(payment_request()->selected_payment_method());
  EXPECT_TRUE(payment_request()->selected_shipping_profile());
  EXPECT_TRUE(payment_request()->selected_shipping_option());
  EXPECT_TRUE(payment_request()->selected_contact_profile());
  EXPECT_TRUE([mediator() canPay]);

  // Payment cannot be completed if there is no selected payment method.
  payments::PaymentApp* selected_payment_method =
      payment_request()->selected_payment_method();
  payment_request()->set_selected_payment_method(nullptr);
  EXPECT_FALSE([mediator() canPay]);

  // Restore the selected payment method.
  payment_request()->set_selected_payment_method(selected_payment_method);
  EXPECT_TRUE([mediator() canPay]);

  // Payment cannot be completed if there is no selected shipping profile,
  // unless no shipping information is requested.
  autofill::AutofillProfile* selected_shipping_profile =
      payment_request()->selected_shipping_profile();
  payment_request()->set_selected_shipping_profile(nullptr);
  EXPECT_FALSE([mediator() canPay]);
  payment_request()->web_payment_request().options.request_shipping = false;
  EXPECT_FALSE([mediator() requestShipping]);
  EXPECT_TRUE([mediator() canPay]);

  // Restore the selected shipping profile and request for shipping information.
  payment_request()->set_selected_shipping_profile(selected_shipping_profile);
  payment_request()->web_payment_request().options.request_shipping = true;
  EXPECT_TRUE([mediator() requestShipping]);
  EXPECT_TRUE([mediator() canPay]);

  // Payment cannot be completed if there is no selected shipping option,
  // unless no shipping information is requested.
  payments::PaymentShippingOption* selected_shipping_option =
      payment_request()->selected_shipping_option();
  payment_request()->set_selected_shipping_option(nullptr);
  EXPECT_FALSE([mediator() canPay]);
  payment_request()->web_payment_request().options.request_shipping = false;
  EXPECT_TRUE([mediator() canPay]);

  // Restore the selected shipping option and request for shipping information.
  payment_request()->set_selected_shipping_option(selected_shipping_option);
  payment_request()->web_payment_request().options.request_shipping = true;
  EXPECT_TRUE([mediator() canPay]);

  // Payment cannot be completed if there is no selected contact profile, unless
  // no contact information is requested.
  payment_request()->set_selected_contact_profile(nullptr);
  EXPECT_FALSE([mediator() canPay]);
  payment_request()->web_payment_request().options.request_payer_name = false;
  EXPECT_TRUE([mediator() requestContactInfo]);
  EXPECT_FALSE([mediator() canPay]);
  payment_request()->web_payment_request().options.request_payer_phone = false;
  EXPECT_TRUE([mediator() requestContactInfo]);
  EXPECT_FALSE([mediator() canPay]);
  payment_request()->web_payment_request().options.request_payer_email = false;
  EXPECT_FALSE([mediator() requestContactInfo]);
  EXPECT_TRUE([mediator() canPay]);
}

// Tests that the Payment Summary item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestPaymentSummaryItem) {
  EXPECT_TRUE([mediator() hasPaymentItems]);

  // Payment Summary item should be of type PriceItem.
  id item = [mediator() paymentSummaryItem];
  ASSERT_TRUE([item isMemberOfClass:[PriceItem class]]);
  PriceItem* payment_summary_item = base::mac::ObjCCastStrict<PriceItem>(item);
  EXPECT_TRUE([payment_summary_item.item isEqualToString:@"Total"]);
  EXPECT_TRUE([payment_summary_item.price isEqualToString:@"USD $1.00"]);
  EXPECT_EQ(nil, payment_summary_item.notification);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            payment_summary_item.accessoryType);

  // A label should indicate if the total value was changed.
  mediator().totalValueChanged = YES;
  item = [mediator() paymentSummaryItem];
  payment_summary_item = base::mac::ObjCCastStrict<PriceItem>(item);
  EXPECT_TRUE([payment_summary_item.notification
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_UPDATED_LABEL)]);

  // The next time the data source is queried for the Payment Summary item, the
  // label should disappear.
  item = [mediator() paymentSummaryItem];
  payment_summary_item = base::mac::ObjCCastStrict<PriceItem>(item);
  EXPECT_EQ(nil, payment_summary_item.notification);

  // Remove the display items.
  payments::WebPaymentRequest web_payment_request =
      payment_request()->web_payment_request();
  web_payment_request.details.display_items.clear();
  payment_request()->UpdatePaymentDetails(web_payment_request.details);
  EXPECT_FALSE([mediator() hasPaymentItems]);

  // No accessory view indicates there are no display items.
  item = [mediator() paymentSummaryItem];
  payment_summary_item = base::mac::ObjCCastStrict<PriceItem>(item);
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            payment_summary_item.accessoryType);
}

// Tests that the Shipping section header item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestShippingHeaderItem) {
  // Shipping section header item should be of type PaymentsTextItem.
  id item = [mediator() shippingSectionHeaderItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* shipping_section_header_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([shipping_section_header_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_SHIPPING_SUMMARY_LABEL)]);
  EXPECT_EQ(nil, shipping_section_header_item.detailText);
}

// Tests that the Shipping Address item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestShippingAddressItem) {
  // Shipping Address item should be of type AutofillProfileItem.
  id item = [mediator() shippingAddressItem];
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* shipping_address_item =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_TRUE([shipping_address_item.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->selected_shipping_profile())]);
  EXPECT_TRUE([shipping_address_item.address
      isEqualToString:GetShippingAddressLabelFromAutofillProfile(
                          *payment_request()->selected_shipping_profile())]);
  EXPECT_TRUE([shipping_address_item.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request()->selected_shipping_profile())]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            shipping_address_item.accessoryType);

  // Reset the selected shipping profile.
  payment_request()->set_selected_shipping_profile(nullptr);

  // When there is no selected shipping address, the Shipping Address item
  // should be of type PaymentsTextItem.
  item = [mediator() shippingAddressItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* add_shipping_address_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([add_shipping_address_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_CHOOSE_SHIPPING_ADDRESS_LABEL)]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            add_shipping_address_item.accessoryType);

  // Remove the shipping profiles.
  payment_request()->ClearShippingProfiles();

  // No shipping profiles to choose from.
  item = [mediator() shippingAddressItem];
  add_shipping_address_item = base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([add_shipping_address_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_ADD_SHIPPING_ADDRESS_LABEL)]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            add_shipping_address_item.accessoryType);
  EXPECT_NE(nil, add_shipping_address_item.trailingImage);
}

// Tests that the Shipping Option item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestShippingOptionItem) {
  // Shipping Option item should be of type PaymentsTextItem.
  id item = [mediator() shippingOptionItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* shipping_option_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([shipping_option_item.text isEqualToString:@"1-Day"]);
  EXPECT_TRUE([shipping_option_item.detailText isEqualToString:@"$0.99"]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            shipping_option_item.accessoryType);

  // Reset the selected shipping option.
  payment_request()->set_selected_shipping_option(nullptr);

  // When there is no selected shipping option, the Shipping Option item should
  // be of type PaymentsTextItem.
  item = [mediator() shippingOptionItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* add_shipping_option_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([add_shipping_option_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_CHOOSE_SHIPPING_OPTION_LABEL)]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            add_shipping_option_item.accessoryType);
}

// Tests that the Payment Method section header item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestPaymentMethodHeaderItem) {
  // Payment Method section header item should be of type PaymentsTextItem.
  id item = [mediator() paymentMethodSectionHeaderItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* payment_method_section_header_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([payment_method_section_header_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME)]);
  EXPECT_EQ(nil, payment_method_section_header_item.detailText);
}

// Tests that the Payment Method item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestPaymentMethodItem) {
  // Payment Method item should be of type PaymentsTextItem.
  id item = [mediator() paymentMethodItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);
  PaymentMethodItem* payment_method_item =
      base::mac::ObjCCastStrict<PaymentMethodItem>(item);
  EXPECT_TRUE([payment_method_item.methodID hasPrefix:@"Visa"]);
  // Last card digits will be preceeded by obfuscation symbols (****) and
  // followed by a Pop Directional Formatting mark; simply check if are part of
  // the payment method ID.
  EXPECT_TRUE([payment_method_item.methodID rangeOfString:@"1111"].location !=
              NSNotFound);
  EXPECT_TRUE([payment_method_item.methodDetail isEqualToString:@"Test User"]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            payment_method_item.accessoryType);

  // Reset the selected payment method.
  payment_request()->set_selected_payment_method(nullptr);

  // When there is no selected payment method, the Payment Method item should be
  // of type PaymentsTextItem.
  item = [mediator() paymentMethodItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* add_payment_method_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([add_payment_method_item.text
      isEqualToString:l10n_util::GetNSString(IDS_CHOOSE_PAYMENT_METHOD)]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            add_payment_method_item.accessoryType);

  // Remove the payment methods.
  payment_request()->ClearPaymentMethods();

  // No payment methods to choose from.
  item = [mediator() paymentMethodItem];
  add_payment_method_item = base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([add_payment_method_item.text
      isEqualToString:l10n_util::GetNSString(IDS_ADD_PAYMENT_METHOD)]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            add_payment_method_item.accessoryType);
  EXPECT_NE(nil, add_payment_method_item.trailingImage);
}

// Tests that the Contact Info section header item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestContactInfoHeaderItem) {
  // Contact Info section header item should be of type PaymentsTextItem.
  id item = [mediator() contactInfoSectionHeaderItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* contact_info_section_header_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([contact_info_section_header_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_CONTACT_DETAILS_LABEL)]);
  EXPECT_EQ(nil, contact_info_section_header_item.detailText);
}

// Tests that the Contact Info item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestContactInfoItem) {
  // Contact Info item should be of type AutofillProfileItem.
  id item = [mediator() contactInfoItem];
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* contact_info_item =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_TRUE([contact_info_item.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->selected_contact_profile())]);
  EXPECT_TRUE([contact_info_item.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request()->selected_contact_profile())]);
  EXPECT_TRUE([contact_info_item.email
      isEqualToString:GetEmailLabelFromAutofillProfile(
                          *payment_request()->selected_contact_profile())]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            contact_info_item.accessoryType);

  // Contact Info item should only show requested fields.
  payment_request()->web_payment_request().options.request_payer_name = false;
  item = [mediator() contactInfoItem];
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  contact_info_item = base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_EQ(nil, contact_info_item.name);
  EXPECT_NE(nil, contact_info_item.phoneNumber);
  EXPECT_NE(nil, contact_info_item.email);

  payment_request()->web_payment_request().options.request_payer_phone = false;
  item = [mediator() contactInfoItem];
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  contact_info_item = base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_EQ(nil, contact_info_item.name);
  EXPECT_EQ(nil, contact_info_item.phoneNumber);
  EXPECT_NE(nil, contact_info_item.email);

  payment_request()->web_payment_request().options.request_payer_name = true;
  payment_request()->web_payment_request().options.request_payer_phone = false;
  payment_request()->web_payment_request().options.request_payer_email = false;
  item = [mediator() contactInfoItem];
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  contact_info_item = base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_NE(nil, contact_info_item.name);
  EXPECT_EQ(nil, contact_info_item.phoneNumber);
  EXPECT_EQ(nil, contact_info_item.email);

  // Reset the selected contact profile.
  payment_request()->set_selected_contact_profile(nullptr);

  // When there is no selected contact profile, the Payment Method item should
  // be of type PaymentsTextItem.
  item = [mediator() contactInfoItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* add_contact_info_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([add_contact_info_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENT_REQUEST_CHOOSE_CONTACT_INFO)]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            add_contact_info_item.accessoryType);

  // Remove the contact profiles.
  payment_request()->ClearContactProfiles();

  // No contact profiles to choose from.
  item = [mediator() contactInfoItem];
  add_contact_info_item = base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([add_contact_info_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENT_REQUEST_ADD_CONTACT_INFO)]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            add_contact_info_item.accessoryType);
  EXPECT_NE(nil, add_contact_info_item.trailingImage);
}

// Tests that the Footer item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestFooterItem) {
  // Make sure the first transaction has not completed yet.
  pref_service()->SetBoolean(payments::kPaymentsFirstTransactionCompleted,
                             false);

  // Make sure the user is signed out.
  ASSERT_FALSE(identity_manager()->HasPrimaryAccount());

  // Footer item should be of type CollectionViewFooterItem.
  id item = [mediator() footerItem];
  ASSERT_TRUE([item isMemberOfClass:[CollectionViewFooterItem class]]);
  CollectionViewFooterItem* footer_item =
      base::mac::ObjCCastStrict<CollectionViewFooterItem>(item);
  EXPECT_TRUE([footer_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS_SIGNED_OUT)]);

  // Fake a signed in user.
  AuthenticationServiceFactory::GetForBrowserState(browser_state())
      ->SignIn(fake_identity());

  item = [mediator() footerItem];
  footer_item = base::mac::ObjCCastStrict<CollectionViewFooterItem>(item);
  EXPECT_TRUE([footer_item.text
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS_SIGNED_IN,
                          base::ASCIIToUTF16("username@gmail.com"))]);

  // Record that the first transaction completed.
  pref_service()->SetBoolean(payments::kPaymentsFirstTransactionCompleted,
                             true);

  item = [mediator() footerItem];
  footer_item = base::mac::ObjCCastStrict<CollectionViewFooterItem>(item);
  EXPECT_TRUE([footer_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS)]);

  // Sign the user out.
  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  AuthenticationServiceFactory::GetForBrowserState(browser_state())
      ->SignOut(signin_metrics::ProfileSignout::SIGNOUT_TEST, ^{
        quit_closure.Run();
      });
  run_loop.Run();

  // The signed in state has no effect on the footer text if the first
  // transaction has completed.
  footer_item = base::mac::ObjCCastStrict<CollectionViewFooterItem>(item);
  EXPECT_TRUE([footer_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS)]);
}
