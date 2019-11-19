// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_mediator.h"

#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const base::Time kOct2017 = base::Time::FromDoubleT(1509050356);

}  // namespace

class PaymentRequestCreditCardEditMediatorTest
    : public PaymentRequestUnitTestBase,
      public PlatformTest {
 protected:
  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
    DoSetUp();

    AddAutofillProfile(autofill::test::GetFullProfile());
    CreateTestPaymentRequest();
  }

  // PlatformTest:
  void TearDown() override {
    DoTearDown();
    PlatformTest::TearDown();
  }
};

// Tests that the expected editor fields are created when creating a card.
TEST_F(PaymentRequestCreditCardEditMediatorTest, TestFieldsWhenCreate) {
  id check_block = ^BOOL(id value) {
    EXPECT_TRUE([value isKindOfClass:[NSArray class]]);
    NSArray* fields = base::mac::ObjCCastStrict<NSArray>(value);
    EXPECT_EQ(5U, fields.count);

    id field = fields[0];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    EditorField* editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeCreditCardNumber, editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_CARD_NUMBER)]);
    EXPECT_EQ(nil, editor_field.value);
    EXPECT_TRUE(editor_field.isRequired);

    field = fields[1];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeCreditCardHolderFullName,
              editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_NAME_ON_CARD)]);
    EXPECT_EQ(nil, editor_field.value);
    EXPECT_TRUE(editor_field.isRequired);

    field = fields[2];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeCreditCardExpDate, editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_EXP_DATE)]);
    NSDateComponents* dateComponents = [[NSCalendar currentCalendar]
        components:NSCalendarUnitMonth | NSCalendarUnitYear
          fromDate:[NSDate date]];
    int currentMonth = [dateComponents month];
    int currentYear = [dateComponents year];
    NSString* currentDate =
        [NSString stringWithFormat:@"%02d / %04d", currentMonth, currentYear];
    EXPECT_TRUE([editor_field.value isEqualToString:currentDate]);
    EXPECT_TRUE(editor_field.isRequired);

    field = fields[3];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeCreditCardBillingAddress,
              editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeSelector, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_BILLING_ADDRESS)]);
    EXPECT_EQ(nil, editor_field.value);
    EXPECT_TRUE(editor_field.isRequired);

    field = fields[4];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeCreditCardSaveToChrome,
              editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeSwitch, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(
                            IDS_PAYMENTS_SAVE_CARD_TO_DEVICE_CHECKBOX)]);
    EXPECT_TRUE([editor_field.value isEqualToString:@"YES"]);
    EXPECT_TRUE(editor_field.isRequired);

    return YES;
  };

  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentRequestEditConsumer)];
  [[consumer expect] setEditorFields:[OCMArg checkWithBlock:check_block]];

  CreditCardEditMediator* mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  creditCard:nil];
  [mediator setConsumer:consumer];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that the expected editor fields are created when editing a card.
TEST_F(PaymentRequestCreditCardEditMediatorTest, TestFieldsWhenEdit) {
  id check_block = ^BOOL(id value) {
    EXPECT_TRUE([value isKindOfClass:[NSArray class]]);
    NSArray* fields = base::mac::ObjCCastStrict<NSArray>(value);
    EXPECT_EQ(5U, fields.count);

    id field = fields[0];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    EditorField* editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"4111 1111 1111 1111"]);

    field = fields[1];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"Test User"]);

    field = fields[2];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"11 / 2022"]);

    field = fields[3];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value
        isEqualToString:base::SysUTF8ToNSString(profiles()[0]->guid())]);

    field = fields[4];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"YES"]);

    return YES;
  };

  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentRequestEditConsumer)];
  [[consumer expect] setEditorFields:[OCMArg checkWithBlock:check_block]];

  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(profiles()[0]->guid());
  CreditCardEditMediator* mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  creditCard:&card];
  [mediator setConsumer:consumer];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that the expected editor fields are created when editing a server card.
TEST_F(PaymentRequestCreditCardEditMediatorTest, TestFieldsWhenEditServerCard) {
  id check_block = ^BOOL(id value) {
    EXPECT_TRUE([value isKindOfClass:[NSArray class]]);
    NSArray* fields = base::mac::ObjCCastStrict<NSArray>(value);
    EXPECT_EQ(1U, fields.count);

    id field = fields[0];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    EditorField* editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value
        isEqualToString:base::SysUTF8ToNSString(profiles()[0]->guid())]);

    return YES;
  };

  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentRequestEditConsumer)];
  [[consumer expect] setEditorFields:[OCMArg checkWithBlock:check_block]];

  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  card.set_billing_address_id(profiles()[0]->guid());
  CreditCardEditMediator* mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  creditCard:&card];
  [mediator setConsumer:consumer];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that no validation error should be expected if validating an empty
// field that is not required.
TEST_F(PaymentRequestCreditCardEditMediatorTest, ValidateEmptyField) {
  CreditCardEditMediator* mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  creditCard:nil];

  EditorField* field = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                   fieldType:EditorFieldTypeTextField
                       label:@""
                       value:@""
                    required:NO];
  NSString* validationError =
      [mediator paymentRequestEditViewController:nil
                                   validateField:(EditorField*)field];
  EXPECT_TRUE(!validationError);
}

// Tests that the appropriate validation error should be expected if validating
// an empty field that is required.
TEST_F(PaymentRequestCreditCardEditMediatorTest, ValidateEmptyRequiredField) {
  CreditCardEditMediator* mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  creditCard:nil];

  EditorField* field = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                   fieldType:EditorFieldTypeTextField
                       label:@""
                       value:@""
                    required:YES];
  NSString* validationError =
      [mediator paymentRequestEditViewController:nil
                                   validateField:(EditorField*)field];
  EXPECT_TRUE([validationError
      isEqualToString:
          l10n_util::GetNSString(
              IDS_PREF_EDIT_DIALOG_FIELD_REQUIRED_VALIDATION_MESSAGE)]);
}

// Tests that the appropriate validation error should be expected if validating
// a field with an invalid value.
TEST_F(PaymentRequestCreditCardEditMediatorTest, ValidateFieldInvalidValue) {
  CreditCardEditMediator* mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  creditCard:nil];

  EditorField* field = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeCreditCardNumber
                   fieldType:EditorFieldTypeTextField
                       label:@""
                       value:@"411111111111111"  // Missing one last digit.
                    required:YES];
  NSString* validationError =
      [mediator paymentRequestEditViewController:nil
                                   validateField:(EditorField*)field];
  EXPECT_TRUE([validationError
      isEqualToString:
          l10n_util::GetNSString(
              IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE)]);

  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kOct2017);

  field = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeCreditCardExpDate
                   fieldType:EditorFieldTypeTextField
                       label:@""
                       value:@"09 / 17"  // September 2017.
                    required:YES];
  validationError =
      [mediator paymentRequestEditViewController:nil
                                   validateField:(EditorField*)field];
  EXPECT_TRUE([validationError
      isEqualToString:
          l10n_util::GetNSString(
              IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED)]);
}

// Tests that the editor's title is correct in various situations.
TEST_F(PaymentRequestCreditCardEditMediatorTest, Title) {
  // No card, so the title should ask to add a card.
  CreditCardEditMediator* mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  creditCard:nil];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_ADD_CARD_LABEL)]);

  const autofill::AutofillProfile& billing_address = *profiles()[0];

  // Complete card, to title should prompt to edit the card.
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  autofill::test::SetCreditCardInfo(&credit_card, nullptr, nullptr, nullptr,
                                    nullptr, billing_address.guid());
  mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  creditCard:&credit_card];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_EDIT_CARD)]);

  // The card's name is missing, so the title should prompt to add the name.
  autofill::test::SetCreditCardInfo(&credit_card, /* name_on_card= */ "",
                                    nullptr, nullptr, nullptr,
                                    billing_address.guid());
  mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  creditCard:&credit_card];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_ADD_NAME_ON_CARD)]);

  // Billing address is also missing, so the title should be generic.
  autofill::test::SetCreditCardInfo(&credit_card, /* name_on_card= */ "",
                                    nullptr, nullptr, nullptr,
                                    /* billing_address_id= */ "");
  mediator =
      [[CreditCardEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  creditCard:&credit_card];
  EXPECT_TRUE(
      [mediator.title isEqualToString:l10n_util::GetNSString(
                                          IDS_PAYMENTS_ADD_MORE_INFORMATION)]);
}
