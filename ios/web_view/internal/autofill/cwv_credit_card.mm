// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/utils/nsobject_description_utils.h"
#include "ui/base/resource/resource_bundle.h"

@interface CWVCreditCard ()

// Gets |value| for |type| from |_internalCard|.
- (NSString*)valueForType:(autofill::FieldType)type;

@end

@implementation CWVCreditCard {
  autofill::CreditCard _internalCard;
}

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard {
  self = [super init];
  if (self) {
    _internalCard = creditCard;
  }
  return self;
}

#pragma mark - Public Methods

- (NSString*)cardHolderFullName {
  return [self valueForType:autofill::CREDIT_CARD_NAME_FULL];
}

- (NSString*)cardNumber {
  return [self valueForType:autofill::CREDIT_CARD_NUMBER];
}

- (NSString*)networkName {
  return [self valueForType:autofill::CREDIT_CARD_TYPE];
}

- (UIImage*)networkIcon {
  int resourceID =
      autofill::CreditCard::IconResourceId(_internalCard.network());
  return ui::ResourceBundle::GetSharedInstance()
      .GetNativeImageNamed(resourceID)
      .ToUIImage();
}

- (NSString*)expirationMonth {
  return [self valueForType:autofill::CREDIT_CARD_EXP_MONTH];
}

- (NSString*)expirationYear {
  return [self valueForType:autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR];
}

- (NSString*)bankName {
  return base::SysUTF8ToNSString(_internalCard.bank_name());
}

#pragma mark - NSObject

- (NSString*)debugDescription {
  NSString* debugDescription = [super debugDescription];
  return [debugDescription
      stringByAppendingFormat:@"\n%@", CWVPropertiesDescription(self)];
}

#pragma mark - Internal

- (autofill::CreditCard*)internalCard {
  return &_internalCard;
}

#pragma mark - Private Methods

- (NSString*)valueForType:(autofill::FieldType)type {
  const std::string& locale =
      ios_web_view::ApplicationContext::GetInstance()->GetApplicationLocale();
  return base::SysUTF16ToNSString(_internalCard.GetInfo(type, locale));
}

@end
