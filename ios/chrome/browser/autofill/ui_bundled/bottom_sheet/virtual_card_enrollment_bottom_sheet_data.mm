// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_data.h"

@implementation VirtualCardEnrollmentBottomSheetData

- (instancetype)initWithCreditCard:(CreditCardData*)creditCard
                             title:(NSString*)title
                explanatoryMessage:(NSString*)explanatoryMessage
                  acceptActionText:(NSString*)acceptActionText
                  cancelActionText:(NSString*)cancelActionText
                 learnMoreLinkText:(NSString*)learnMoreLinkText
           googleLegalMessageLines:(NSArray<SaveCardMessageWithLinks*>*)
                                       paymentServerLegalMessageLines
           issuerLegalMessageLines:
               (NSArray<SaveCardMessageWithLinks*>*)issuerLegalMessageLines {
  self = [super init];
  if (self) {
    _creditCard = creditCard;
    _title = title;
    _explanatoryMessage = explanatoryMessage;
    _acceptActionText = acceptActionText;
    _cancelActionText = cancelActionText;
    _learnMoreLinkText = learnMoreLinkText;
    _paymentServerLegalMessageLines = paymentServerLegalMessageLines;
    _issuerLegalMessageLines = issuerLegalMessageLines;
  }
  return self;
}

@end
