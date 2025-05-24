// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_util.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_type.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_type_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/text_view_util.h"

@implementation AutofillCreditCardUtil

+ (autofill::CreditCard)creditCardWithHolderName:(NSString*)cardHolderName
                                      cardNumber:(NSString*)cardNumber
                                 expirationMonth:(NSString*)expirationMonth
                                  expirationYear:(NSString*)expirationYear
                                    cardNickname:(NSString*)cardNickname
                                        appLocal:(const std::string&)appLocal {
  autofill::CreditCard creditCard = autofill::CreditCard();
  [self updateCreditCard:&creditCard
          cardHolderName:cardHolderName
              cardNumber:cardNumber
         expirationMonth:expirationMonth
          expirationYear:expirationYear
            cardNickname:cardNickname
                appLocal:appLocal];
  return creditCard;
}

+ (bool)isValidCreditCard:(NSString*)cardNumber
          expirationMonth:(NSString*)expirationMonth
           expirationYear:(NSString*)expirationYear
             cardNickname:(NSString*)cardNickname
                 appLocal:(const std::string&)appLocal {
  return ([self isValidCreditCardNumber:cardNumber appLocal:appLocal] &&
          [self isValidCreditCardExpirationMonth:expirationMonth] &&
          [self isValidCreditCardExpirationYear:expirationYear
                                       appLocal:appLocal] &&
          [self isValidCardNickname:cardNickname]);
}

+ (void)updateCreditCard:(autofill::CreditCard*)creditCard
          cardHolderName:(NSString*)cardHolderName
              cardNumber:(NSString*)cardNumber
         expirationMonth:(NSString*)expirationMonth
          expirationYear:(NSString*)expirationYear
            cardNickname:(NSString*)cardNickname
                appLocal:(const std::string&)appLocal {
  [self updateCreditCard:creditCard
                  cardProperty:cardHolderName
      autofillCreditCardUIType:AutofillCreditCardUIType::kFullName
                      appLocal:appLocal];

  [self updateCreditCard:creditCard
                  cardProperty:cardNumber
      autofillCreditCardUIType:AutofillCreditCardUIType::kNumber
                      appLocal:appLocal];

  [self updateCreditCard:creditCard
                  cardProperty:expirationMonth
      autofillCreditCardUIType:AutofillCreditCardUIType::kExpMonth
                      appLocal:appLocal];

  [self updateCreditCard:creditCard
                  cardProperty:expirationYear
      autofillCreditCardUIType:AutofillCreditCardUIType::kExpYear
                      appLocal:appLocal];

  creditCard->SetNickname(base::SysNSStringToUTF16(cardNickname));
}

+ (BOOL)isValidCreditCardNumber:(NSString*)cardNumber
                       appLocal:(const std::string&)appLocal {
  autofill::CreditCard creditCard = [self creditCardWithHolderName:nil
                                                        cardNumber:cardNumber
                                                   expirationMonth:nil
                                                    expirationYear:nil
                                                      cardNickname:nil
                                                          appLocal:appLocal];
  return creditCard.HasValidCardNumber();
}

+ (BOOL)isValidCreditCardExpirationMonth:(NSString*)expirationMonth {
  return ([expirationMonth integerValue] >= 1 &&
          [expirationMonth integerValue] <= 12);
}

+ (BOOL)isValidCreditCardExpirationYear:(NSString*)expirationYear
                               appLocal:(const std::string&)appLocal {
  autofill::CreditCard creditCard =
      [self creditCardWithHolderName:nil
                          cardNumber:nil
                     expirationMonth:nil
                      expirationYear:expirationYear
                        cardNickname:nil
                            appLocal:appLocal];
  return creditCard.HasValidExpirationYear();
}

+ (BOOL)isValidCardNickname:(NSString*)cardNickname {
  return autofill::CreditCard::IsNicknameValid(
      base::SysNSStringToUTF16(cardNickname));
}

+ (BOOL)shouldEditCardFromPaymentsWebPage:(const autofill::CreditCard&)card {
  switch (card.record_type()) {
    case autofill::CreditCard::RecordType::kLocalCard:
    case autofill::CreditCard::RecordType::kVirtualCard:
      return NO;
    case autofill::CreditCard::RecordType::kMaskedServerCard:
      return YES;
    case autofill::CreditCard::RecordType::kFullServerCard:
      // Full server cards are a temporary cached state and should not be
      // offered for edit (from payments web page or otherwise).
      NOTREACHED();
  }
}

+ (UITextView*)createTextViewForLegalMessage:
    (SaveCardMessageWithLinks*)legalMessage {
  UITextView* textView = CreateUITextViewWithTextKit1();
  textView.scrollEnabled = NO;
  textView.editable = NO;
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  textView.textContainerInset = UIEdgeInsetsZero;
  textView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
  textView.backgroundColor = UIColor.clearColor;
  textView.attributedText =
      [AutofillCreditCardUtil setAttributedText:legalMessage.messageText
                                       linkUrls:legalMessage.linkURLs
                                     linkRanges:legalMessage.linkRanges];
  return textView;
}

#pragma mark - Private

// Updates the `AutofillUIType` of the `creditCard` with the value of
// `cardProperty`.
+ (void)updateCreditCard:(autofill::CreditCard*)creditCard
                cardProperty:(NSString*)cardValue
    autofillCreditCardUIType:(AutofillCreditCardUIType)autofillCreditCardUIType
                    appLocal:(const std::string&)appLocal {
  creditCard->SetInfo(
      autofill::AutofillType(
          AutofillTypeFromAutofillUITypeForCard(autofillCreditCardUIType)),
      base::SysNSStringToUTF16(cardValue), appLocal);
}

// Creates a string with hyperlinks.
+ (NSAttributedString*)setAttributedText:(NSString*)text
                                linkUrls:(std::vector<GURL>)linkURLs
                              linkRanges:(NSArray*)linkRanges {
  CHECK(linkRanges.count == linkURLs.size());
  NSMutableParagraphStyle* centeredTextStyle =
      [[NSMutableParagraphStyle alloc] init];
  centeredTextStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSParagraphStyleAttributeName : centeredTextStyle,
  };

  // TODO(crbug.com/413051428): Add a utility function in string_util that
  // applies a link to a given range in an NSMutableAttributedString.
  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:text
                                             attributes:textAttributes];
  if (linkRanges) {
    [linkRanges enumerateObjectsUsingBlock:^(NSValue* rangeValue, NSUInteger i,
                                             BOOL* stop) {
      CrURL* crurl = [[CrURL alloc] initWithGURL:linkURLs[i]];
      if (!crurl || !crurl.gurl.is_valid()) {
        return;
      }
      NSDictionary* linkAttributes = @{
        NSLinkAttributeName : crurl.nsurl,
        NSFontAttributeName : PreferredFontForTextStyle(UIFontTextStyleCaption2,
                                                        UIFontWeightSemibold)
      };
      [attributedText addAttributes:linkAttributes range:rangeValue.rangeValue];
    }];
  }
  return attributedText;
}

@end
