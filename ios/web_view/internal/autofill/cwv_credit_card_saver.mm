// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_credit_card_saver_internal.h"

#include <memory>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "net/base/mac/url_conversions.h"
#include "ui/gfx/range/range.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  autofill::AutofillClient::SaveCreditCardOptions _saveOptions;
  autofill::AutofillClient::UploadSaveCardPromptCallback
      _uploadSaveCardCallback;
  autofill::AutofillClient::LocalSaveCardPromptCallback _localSaveCardCallback;

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
@synthesize willUploadToCloud = _willUploadToCloud;

- (instancetype)
          initWithCreditCard:(const autofill::CreditCard&)creditCard
                 saveOptions:(autofill::AutofillClient::SaveCreditCardOptions)
                                 saveOptions
           willUploadToCloud:(BOOL)willUploadToCloud
           legalMessageLines:(autofill::LegalMessageLines)legalMessageLines
    uploadSavePromptCallback:
        (autofill::AutofillClient::UploadSaveCardPromptCallback)
            uploadSavePromptCallback
     localSavePromptCallback:
         (autofill::AutofillClient::LocalSaveCardPromptCallback)
             localSavePromptCallback {
  self = [super init];
  if (self) {
    _creditCard = [[CWVCreditCard alloc] initWithCreditCard:creditCard];
    _saveOptions = saveOptions;
    _willUploadToCloud = willUploadToCloud;
    _legalMessages = CWVLegalMessagesFromLegalMessageLines(legalMessageLines);
    _uploadSaveCardCallback = std::move(uploadSavePromptCallback);
    _localSaveCardCallback = std::move(localSavePromptCallback);
    _decisionMade = NO;
  }
  return self;
}

- (void)dealloc {
  // If the user did not choose, the decision should be marked as ignored.
  if (_willUploadToCloud && _uploadSaveCardCallback) {
    std::move(_uploadSaveCardCallback)
        .Run(autofill::AutofillClient::IGNORED,
             /*user_provided_card_details=*/{});
  } else if (!_willUploadToCloud && _localSaveCardCallback) {
    std::move(_localSaveCardCallback).Run(autofill::AutofillClient::IGNORED);
  }
}

#pragma mark - Public Methods

- (void)acceptWithRiskData:(nullable NSString*)riskData
         completionHandler:(void (^_Nullable)(BOOL))completionHandler {
  DCHECK(!_decisionMade)
      << "You may only call -acceptWithRiskData:completionHandler: or "
         "-decline: once per instance.";
  if (_willUploadToCloud) {
    DCHECK(_riskDataCallback && riskData);
    std::move(_riskDataCallback).Run(base::SysNSStringToUTF8(riskData));

    _saveCompletionHandler = completionHandler;
    DCHECK(_uploadSaveCardCallback);
    std::move(_uploadSaveCardCallback)
        .Run(autofill::AutofillClient::ACCEPTED,
             /*user_provided_card_details=*/{});
  } else {
    DCHECK(_localSaveCardCallback);
    std::move(_localSaveCardCallback).Run(autofill::AutofillClient::ACCEPTED);
    base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                     if (completionHandler) {
                       completionHandler(YES);
                     }
                   }));
  }
  _decisionMade = YES;
}

- (void)decline {
  DCHECK(!_decisionMade)
      << "You may only call -acceptWithRiskData:completionHandler: or "
         "-decline: once per instance.";
  if (_willUploadToCloud) {
    DCHECK(_uploadSaveCardCallback);
    std::move(_uploadSaveCardCallback)
        .Run(autofill::AutofillClient::DECLINED,
             /*user_provided_card_details=*/{});
  } else {
    DCHECK(_localSaveCardCallback);
    std::move(_localSaveCardCallback).Run(autofill::AutofillClient::DECLINED);
  }
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
