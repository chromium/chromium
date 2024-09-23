// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_saver_internal.h"
#import "net/base/apple/url_conversions.h"
#include "ui/gfx/range/range.h"

NS_ASSUME_NONNULL_BEGIN

namespace {
// Converts |autofill::LegalMessageLines| into |NSArray<NSAttributedString*>*|.
NSArray<NSAttributedString*>* CWVLegalMessagesFromLegalMessageLines(
    const autofill::LegalMessageLines& legalMessageLines) {
  NSMutableArray<NSAttributedString*>* legalMessages = [NSMutableArray array];
  for (const autofill::LegalMessageLine& legalMessageLine : legalMessageLines) {
    NSString* text = base::SysUTF16ToNSString(legalMessageLine.text());
    NSMutableAttributedString* legalMessage =
        [[NSMutableAttributedString alloc] initWithString:text];
    for (const autofill::LegalMessageLine::Link& link :
         legalMessageLine.links()) {
      NSURL* url = net::NSURLWithGURL(link.url);
      NSRange range = link.range.ToNSRange();
      [legalMessage addAttribute:NSLinkAttributeName value:url range:range];
    }
    [legalMessages addObject:[legalMessage copy]];
  }
  return [legalMessages copy];
}
}  // namespace

@implementation CWVCreditCardSaver {
  autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions
      _saveOptions;
  autofill::payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      _saveCardCallback;

  // The callback to invoke for save completion results.
  void (^_Nullable _saveCompletionHandler)(BOOL);

  // The callback to invoke for returning risk data.
  base::OnceCallback<void(const std::string&)> _riskDataCallback;

  // Whether or not either |acceptCreditCardWithRiskData:completionHandler:| or
  // |declineCreditCardSave| has been called.
  BOOL _decisionMade;
}

@synthesize creditCard = _creditCard;
@synthesize legalMessages = _legalMessages;

- (instancetype)
    initWithCreditCard:(const autofill::CreditCard&)creditCard
           saveOptions:(autofill::payments::PaymentsAutofillClient::
                            SaveCreditCardOptions)saveOptions
     legalMessageLines:(autofill::LegalMessageLines)legalMessageLines
    savePromptCallback:(autofill::payments::PaymentsAutofillClient::
                            UploadSaveCardPromptCallback)savePromptCallback {
  self = [super init];
  if (self) {
    _creditCard = [[CWVCreditCard alloc] initWithCreditCard:creditCard];
    _saveOptions = saveOptions;
    _legalMessages = CWVLegalMessagesFromLegalMessageLines(legalMessageLines);
    _saveCardCallback = std::move(savePromptCallback);
    _decisionMade = NO;
  }
  return self;
}

- (void)dealloc {
  // If the user did not choose, the decision should be marked as ignored.
  if (_saveCardCallback) {
    std::move(_saveCardCallback)
        .Run(autofill::payments::PaymentsAutofillClient::
                 SaveCardOfferUserDecision::kIgnored,
             /*user_provided_card_details=*/{});
  }
}

#pragma mark - Public Methods

- (void)acceptWithCardHolderFullName:(NSString*)cardHolderFullName
                     expirationMonth:(NSString*)expirationMonth
                      expirationYear:(NSString*)expirationYear
                            riskData:(NSString*)riskData
                   completionHandler:(void (^)(BOOL))completionHandler {
  DCHECK(!_decisionMade)
      << "You may only call -acceptWithRiskData:completionHandler: or "
         "-decline: once per instance.";
  DCHECK(_riskDataCallback && riskData);
  std::move(_riskDataCallback).Run(base::SysNSStringToUTF8(riskData));

  _saveCompletionHandler = completionHandler;
  DCHECK(_saveCardCallback);
  std::move(_saveCardCallback)
      .Run(autofill::payments::PaymentsAutofillClient::
               SaveCardOfferUserDecision::kAccepted,
           {
               .cardholder_name = base::SysNSStringToUTF16(cardHolderFullName),
               .expiration_date_month =
                   base::SysNSStringToUTF16(expirationMonth),
               .expiration_date_year = base::SysNSStringToUTF16(expirationYear),
           });
  _decisionMade = YES;
}

- (void)decline {
  DCHECK(!_decisionMade)
      << "You may only call -acceptWithRiskData:completionHandler: or "
         "-decline: once per instance.";
  DCHECK(_saveCardCallback);
  std::move(_saveCardCallback)
      .Run(autofill::payments::PaymentsAutofillClient::
               SaveCardOfferUserDecision::kDeclined,
           /*user_provided_card_details=*/{});
  _decisionMade = YES;
}

#pragma mark - Internal Methods

- (void)handleCreditCardUploadCompleted:(BOOL)cardSaved {
  if (_saveCompletionHandler) {
    _saveCompletionHandler(cardSaved);
    _saveCompletionHandler = nil;
  }
}

- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback {
  _riskDataCallback = std::move(callback);
}

@end

NS_ASSUME_NONNULL_END
