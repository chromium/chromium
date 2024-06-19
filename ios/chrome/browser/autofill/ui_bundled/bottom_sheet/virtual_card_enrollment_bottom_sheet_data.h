// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_DATA_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_DATA_H_

#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"

// A value object of properties shown in the enrollment prompt.
@interface VirtualCardEnrollmentBottomSheetData : NSObject

// The credit card that would be enroll as a virtual card.
@property(readonly, strong) CreditCardData* creditCard;

// The title of the enrollment prompt.
@property(readonly, strong) NSString* title;

// The explanatory message of the prompt.
@property(readonly, strong) NSString* explanatoryMessage;

// The label text for accepting the prompt (i.e. the accept button label).
@property(readonly, strong) NSString* acceptActionText;

// The label text for cancelling the prompt (i.e. the decline button label).
@property(readonly, strong) NSString* cancelActionText;

// A substring of the explanatory message that should link to the learn more
// help page.
@property(readonly, strong) NSString* learnMoreLinkText;

// The legal message from the payment server.
// TODO(crbug.com/40282545): Rename SaveCardMessageWithLinks to
// LegalMessageLine.
@property(readonly, strong)
    NSArray<SaveCardMessageWithLinks*>* paymentServerLegalMessageLines;

// The legal message from the issuer.
@property(readonly, strong)
    NSArray<SaveCardMessageWithLinks*>* issuerLegalMessageLines;

- (instancetype)init NS_UNAVAILABLE;

// Constructs this object from its properties.
- (instancetype)initWithCreditCard:(CreditCardData*)creditCard
                             title:(NSString*)title
                explanatoryMessage:(NSString*)explanatoryMessage
                  acceptActionText:(NSString*)acceptActionText
                  cancelActionText:(NSString*)cancelActionText
                 learnMoreLinkText:(NSString*)learnMoreLinkText
           googleLegalMessageLines:
               (NSArray<SaveCardMessageWithLinks*>*)googleLegalMessageLines
           issuerLegalMessageLines:
               (NSArray<SaveCardMessageWithLinks*>*)issuerLegalMessageLines
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_DATA_H_
