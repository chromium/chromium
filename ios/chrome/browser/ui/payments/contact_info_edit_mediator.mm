// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/contact_info_edit_mediator.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/validation.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContactInfoEditMediator ()

// The PaymentRequest object owning an instance of payments::WebPaymentRequest
// as provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

// The profile to be edited, if any. This pointer is not owned by this class and
// should outlive it.
@property(nonatomic, assign) autofill::AutofillProfile* profile;

// The list of current editor fields.
@property(nonatomic, strong) NSMutableArray<EditorField*>* fields;

@end

@implementation ContactInfoEditMediator

@synthesize state = _state;
@synthesize consumer = _consumer;
@synthesize paymentRequest = _paymentRequest;
@synthesize profile = _profile;
@synthesize fields = _fields;

- (instancetype)initWithPaymentRequest:(payments::PaymentRequest*)paymentRequest
                               profile:(autofill::AutofillProfile*)profile {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
    _profile = profile;
    _state =
        _profile ? EditViewControllerStateEdit : EditViewControllerStateCreate;
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<PaymentRequestEditConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setEditorFields:[self createEditorFields]];
}

#pragma mark - PaymentRequestEditViewControllerDataSource

- (NSString*)title {
  if (!self.profile)
    return l10n_util::GetNSString(IDS_PAYMENTS_ADD_CONTACT_DETAILS_LABEL);

  if (self.paymentRequest->profile_comparator()->IsContactInfoComplete(
          self.profile)) {
    return l10n_util::GetNSString(IDS_PAYMENTS_EDIT_CONTACT_DETAILS_LABEL);
  }

  return base::SysUTF16ToNSString(
      self.paymentRequest->profile_comparator()
          ->GetTitleForMissingContactFields(*self.profile));
}

- (CollectionViewItem*)headerItem {
  return nil;
}

- (BOOL)shouldHideBackgroundForHeaderItem {
  return NO;
}

- (BOOL)shouldFormatValueForAutofillUIType:(AutofillUIType)type {
  return (type == AutofillUITypeProfileHomePhoneWholeNumber);
}

- (NSString*)formatValue:(NSString*)value autofillUIType:(AutofillUIType)type {
  if (type == AutofillUITypeProfileHomePhoneWholeNumber) {
    const std::string countryCode =
        autofill::AutofillCountry::CountryCodeForLocale(
            _paymentRequest->GetApplicationLocale());
    return base::SysUTF8ToNSString(autofill::i18n::FormatPhoneForDisplay(
        base::SysNSStringToUTF8(value), countryCode));
  }
  return nil;
}

- (UIImage*)iconIdentifyingEditorField:(EditorField*)field {
  return nil;
}

#pragma mark - PaymentRequestEditViewControllerValidator

- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateField:(EditorField*)field {
  if (field.value.length) {
    switch (field.autofillUIType) {
      case AutofillUITypeProfileHomePhoneWholeNumber: {
        const std::string countryCode =
            autofill::AutofillCountry::CountryCodeForLocale(
                self.paymentRequest->GetApplicationLocale());
        if (!autofill::IsPossiblePhoneNumber(
                base::SysNSStringToUTF16(field.value), countryCode)) {
          return l10n_util::GetNSString(
              IDS_PAYMENTS_PHONE_INVALID_VALIDATION_MESSAGE);
        }
        break;
      }
      case AutofillUITypeProfileEmailAddress: {
        if (!autofill::IsValidEmailAddress(
                base::SysNSStringToUTF16(field.value))) {
          return l10n_util::GetNSString(
              IDS_PAYMENTS_EMAIL_INVALID_VALIDATION_MESSAGE);
        }
        break;
      }
      default:
        break;
    }
  } else if (field.isRequired) {
    return l10n_util::GetNSString(
        IDS_PREF_EDIT_DIALOG_FIELD_REQUIRED_VALIDATION_MESSAGE);
  }
  return nil;
}

#pragma mark - Helper methods

// Creates and returns an array of editor fields.
- (NSArray<EditorField*>*)createEditorFields {
  self.fields = [[NSMutableArray alloc] init];

  if (_paymentRequest->request_payer_name()) {
    NSString* name =
        [self fieldValueFromProfile:self.profile fieldType:autofill::NAME_FULL];
    EditorField* nameField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileFullName
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(
                                   IDS_PAYMENTS_NAME_FIELD_IN_CONTACT_DETAILS)
                         value:name
                      required:YES];
    if (!_paymentRequest->request_payer_phone() &&
        !_paymentRequest->request_payer_email()) {
      nameField.returnKeyType = UIReturnKeyDone;
    }
    [self.fields addObject:nameField];
  }

  if (_paymentRequest->request_payer_phone()) {
    NSString* phone =
        self.profile
            ? base::SysUTF16ToNSString(
                  autofill::i18n::GetFormattedPhoneNumberForDisplay(
                      *self.profile, _paymentRequest->GetApplicationLocale()))
            : nil;
    EditorField* phoneField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(
                                   IDS_PAYMENTS_PHONE_FIELD_IN_CONTACT_DETAILS)
                         value:phone
                      required:YES];
    phoneField.keyboardType = UIKeyboardTypePhonePad;
    if (!_paymentRequest->request_payer_email())
      phoneField.returnKeyType = UIReturnKeyDone;
    [self.fields addObject:phoneField];
  }

  if (_paymentRequest->request_payer_email()) {
    NSString* email = [self fieldValueFromProfile:self.profile
                                        fieldType:autofill::EMAIL_ADDRESS];
    EditorField* emailField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileEmailAddress
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(
                                   IDS_PAYMENTS_EMAIL_FIELD_IN_CONTACT_DETAILS)
                         value:email
                      required:YES];
    emailField.keyboardType = UIKeyboardTypeEmailAddress;
    emailField.autoCapitalizationType = UITextAutocapitalizationTypeNone;
    emailField.returnKeyType = UIReturnKeyDone;
    [self.fields addObject:emailField];
  }

  DCHECK(self.fields.count > 0) << "The contact info editor shouldn't be "
                                   "reachable if no contact information is "
                                   "requested.";
  return self.fields;
}

// Takes in an autofill profile and an autofill field type and returns the
// corresponding field value. Returns nil if |profile| is nullptr.
- (NSString*)fieldValueFromProfile:(autofill::AutofillProfile*)profile
                         fieldType:(autofill::ServerFieldType)fieldType {
  return profile ? base::SysUTF16ToNSString(profile->GetInfo(
                       autofill::AutofillType(fieldType),
                       _paymentRequest->GetApplicationLocale()))
                 : nil;
}

@end
