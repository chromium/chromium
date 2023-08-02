// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_form_internal.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/dense_set.h"

@implementation CWVAutofillForm

- (instancetype)initWithFormStructure:
    (const autofill::FormStructure&)formStructure {
  self = [super init];
  if (self) {
    _name = base::SysUTF16ToNSString(formStructure.form_name());

    _type = CWVAutofillFormTypeUnknown;
    autofill::DenseSet<autofill::FormType> formTypes =
        formStructure.GetFormTypes();
    if (formTypes.contains(autofill::FormType::kAddressForm)) {
      _type |= CWVAutofillFormTypeAddresses;
    }
    if (formTypes.contains(autofill::FormType::kCreditCardForm)) {
      _type |= CWVAutofillFormTypeCreditCards;
    }
    // Underlying autofill code does not parse password fields because it does
    // not consider password forms as autofillable. In other words, |formTypes|
    // will never contain PASSWORD_FORM. Luckily, it already provides a function
    // to check if it has any password fields.
    if (formStructure.has_password_field()) {
      _type |= CWVAutofillFormTypePasswords;
    }
  }
  return self;
}

#pragma mark - NSObject

- (NSString*)debugDescription {
  NSString* description = [super debugDescription];
  NSMutableArray<NSString*>* descriptionStrings =
      [NSMutableArray arrayWithObject:description];
  if (_name) {
    [descriptionStrings addObject:_name];
  }
  if (_type == CWVAutofillFormTypeUnknown) {
    [descriptionStrings addObject:@"Unknown"];
  } else {
    if (_type & CWVAutofillFormTypeAddresses) {
      [descriptionStrings addObject:@"Addresses"];
    }
    if (_type & CWVAutofillFormTypeCreditCards) {
      [descriptionStrings addObject:@"Credit cards"];
    }
    if (_type & CWVAutofillFormTypePasswords) {
      [descriptionStrings addObject:@"Passwords"];
    }
  }
  return [descriptionStrings componentsJoinedByString:@", "];
}

@end
