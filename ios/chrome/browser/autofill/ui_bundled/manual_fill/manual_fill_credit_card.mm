// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credit_card.h"

@implementation ManualFillCreditCard

- (instancetype)initWithGUID:(NSString*)GUID
                     network:(NSString*)network
                        icon:(UIImage*)icon
                    bankName:(NSString*)bankName
                  cardHolder:(NSString*)cardHolder
                      number:(NSString*)number
            obfuscatedNumber:(NSString*)obfuscatedNumber
    networkAndLastFourDigits:(NSString*)networkAndLastFourDigits
              expirationYear:(NSString*)expirationYear
             expirationMonth:(NSString*)expirationMonth
                         CVC:(NSString*)CVC
                  recordType:(autofill::CreditCard::RecordType)recordType
             canFillDirectly:(BOOL)canFillDirectly {
  self = [super init];
  if (self) {
    _GUID = [GUID copy];
    _network = [network copy];
    _icon = icon;
    _bankName = [bankName copy];
    _cardHolder = [cardHolder copy];
    _number = [number copy];
    _obfuscatedNumber = [obfuscatedNumber copy];
    _networkAndLastFourDigits = [networkAndLastFourDigits copy];
    _expirationYear = [expirationYear copy];
    _expirationMonth = [expirationMonth copy];
    _CVC = [CVC copy];
    _recordType = recordType;
    _canFillDirectly = canFillDirectly;
  }
  return self;
}

- (BOOL)isEqual:(id)object {
  if (!object) {
    return NO;
  }
  if (self == object) {
    return YES;
  }
  if (![object isMemberOfClass:[ManualFillCreditCard class]]) {
    return NO;
  }
  ManualFillCreditCard* otherObject = (ManualFillCreditCard*)object;
  // Guid and number matches or not, there's no need to care about the other
  // fields. 'Number' differenciate between the same card obfuscated or not.
  if (![otherObject.GUID isEqualToString:self.GUID]) {
    return NO;
  }
  if (![otherObject.number isEqualToString:self.number]) {
    return NO;
  }
  return YES;
}

- (NSUInteger)hash {
  return [self.GUID hash] ^ [self.number hash];
}

- (NSString*)description {
  // Not returning raw number, just obfuscated number, in case this ends up in
  // logs.
  return [NSString
      stringWithFormat:@"<%@ (%p): GUID: %@, network: %@, "
                       @"bankName: %@, cardHolder: %@, obfuscatedNumber: %@, "
                       @"expirationYear: %@, expirationMonth: %@>",
                       NSStringFromClass([self class]), self, self.GUID,
                       self.network, self.bankName, self.cardHolder,
                       self.obfuscatedNumber, self.expirationYear,
                       self.expirationMonth];
}
@end
