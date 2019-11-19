// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_mediator.h"

#include "base/mac/foundation_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/test_region_data_loader.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/platform_test.h"
#include "third_party/libaddressinput/messages.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestAddressEditMediatorTest : public PaymentRequestUnitTestBase,
                                              public PlatformTest {
 protected:
  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
    DoSetUp();

    autofill::CountryNames::SetLocaleString("en-US");

    CreateTestPaymentRequest();

    test_region_data_loader_.set_synchronous_callback(true);
    payment_request()->SetRegionDataLoader(&test_region_data_loader_);
  }

  // PlatformTest:
  void TearDown() override {
    DoTearDown();
    PlatformTest::TearDown();
  }

  autofill::TestRegionDataLoader test_region_data_loader_;
};

// Tests that the expected editor fields are created when creating an address.
TEST_F(PaymentRequestAddressEditMediatorTest, TestFieldsWhenCreate) {
  id check_block = ^BOOL(id value) {
    EXPECT_TRUE([value isKindOfClass:[NSArray class]]);
    NSArray* fields = base::mac::ObjCCastStrict<NSArray>(value);
    EXPECT_EQ(8U, fields.count);

    id field = fields[0];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    EditorField* editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileFullName, editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(
                            IDS_LIBADDRESSINPUT_RECIPIENT_LABEL)]);
    EXPECT_EQ(nil, editor_field.value);
    EXPECT_FALSE(editor_field.isRequired);

    field = fields[1];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileHomeAddressCountry,
              editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeSelector, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(
                            IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL)]);
    EXPECT_TRUE([editor_field.value isEqualToString:@"US"]);
    EXPECT_TRUE(editor_field.isRequired);

    field = fields[2];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileCompanyName, editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(
                            IDS_LIBADDRESSINPUT_ORGANIZATION_LABEL)]);
    EXPECT_EQ(nil, editor_field.value);
    EXPECT_FALSE(editor_field.isRequired);

    field = fields[3];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileHomeAddressStreet,
              editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(
                            IDS_LIBADDRESSINPUT_ADDRESS_LINE_1_LABEL)]);
    EXPECT_EQ(nil, editor_field.value);
    EXPECT_TRUE(editor_field.isRequired);

    field = fields[4];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileHomeAddressCity,
              editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(
                            IDS_LIBADDRESSINPUT_LOCALITY_LABEL)]);
    EXPECT_EQ(nil, editor_field.value);
    EXPECT_TRUE(editor_field.isRequired);

    field = fields[5];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileHomeAddressState,
              editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(IDS_LIBADDRESSINPUT_STATE)]);
    EXPECT_TRUE([editor_field.value
        isEqualToString:l10n_util::GetNSString(IDS_AUTOFILL_LOADING_REGIONS)]);
    EXPECT_TRUE(editor_field.isRequired);

    field = fields[6];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileHomeAddressZip, editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(
                            IDS_LIBADDRESSINPUT_ZIP_CODE_LABEL)]);
    EXPECT_EQ(nil, editor_field.value);
    EXPECT_TRUE(editor_field.isRequired);

    field = fields[7];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileHomePhoneWholeNumber,
              editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(IDS_IOS_AUTOFILL_PHONE)]);
    EXPECT_EQ(nil, editor_field.value);
    EXPECT_TRUE(editor_field.isRequired);

    return YES;
  };

  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentRequestEditConsumer)];
  [[consumer expect] setEditorFields:[OCMArg checkWithBlock:check_block]];

  AddressEditMediator* mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:nil];
  [mediator setConsumer:consumer];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that the expected editor fields are created when editing an address.
TEST_F(PaymentRequestAddressEditMediatorTest, TestFieldsWhenEdit) {
  id check_block = ^BOOL(id value) {
    EXPECT_TRUE([value isKindOfClass:[NSArray class]]);
    NSArray* fields = base::mac::ObjCCastStrict<NSArray>(value);
    EXPECT_EQ(8U, fields.count);

    id field = fields[0];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    EditorField* editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"John H. Doe"]);

    field = fields[1];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"US"]);

    field = fields[2];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"Underworld"]);

    field = fields[3];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"666 Erebus St.\nApt 8"]);

    field = fields[4];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"Elysium"]);

    field = fields[5];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value
        isEqualToString:l10n_util::GetNSString(IDS_AUTOFILL_LOADING_REGIONS)]);

    field = fields[6];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"91111"]);

    field = fields[7];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"+1 650-211-1111"]);

    return YES;
  };

  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentRequestEditConsumer)];
  [[consumer expect] setEditorFields:[OCMArg checkWithBlock:check_block]];

  autofill::AutofillProfile autofill_profile = autofill::test::GetFullProfile();
  AddressEditMediator* mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:&autofill_profile];
  [mediator setConsumer:consumer];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that no validation error should be expected if validating an empty
// field that is not required.
TEST_F(PaymentRequestAddressEditMediatorTest, ValidateEmptyField) {
  AddressEditMediator* mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:nil];

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
TEST_F(PaymentRequestAddressEditMediatorTest, ValidateEmptyRequiredField) {
  AddressEditMediator* mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:nil];

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
TEST_F(PaymentRequestAddressEditMediatorTest, ValidateFieldInvalidValue) {
  AddressEditMediator* mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:nil];

  EditorField* field = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                   fieldType:EditorFieldTypeTextField
                       label:@""
                       value:@"15068531"  // It is too short.
                    required:YES];
  NSString* validationError =
      [mediator paymentRequestEditViewController:nil
                                   validateField:(EditorField*)field];
  EXPECT_TRUE([validationError
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_PHONE_INVALID_VALIDATION_MESSAGE)]);
}

// Tests that the editor's title is correct in various situations.
TEST_F(PaymentRequestAddressEditMediatorTest, Title) {
  // No address, so the title should ask to add an address.
  AddressEditMediator* mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:nil];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_ADD_ADDRESS)]);

  // Complete address, to title should prompt to edit the address.
  autofill::AutofillProfile autofill_profile = autofill::test::GetFullProfile();
  mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:&autofill_profile];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_EDIT_ADDRESS)]);

  // Some address fields are missing, so title should ask to add a valid
  // address.
  payment_request()->profile_comparator()->Invalidate(autofill_profile);
  autofill::test::SetProfileInfo(
      &autofill_profile, nullptr, nullptr, nullptr, nullptr, nullptr,
      /* address1= */ "", /* address2= */ "",
      /* city= */ "", nullptr, nullptr, /* country= */ "", nullptr);
  mediator =
      [[AddressEditMediator alloc] initWithPaymentRequest:payment_request()
                                                  address:&autofill_profile];
  EXPECT_TRUE([mediator.title
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_ADD_VALID_ADDRESS)]);
}
