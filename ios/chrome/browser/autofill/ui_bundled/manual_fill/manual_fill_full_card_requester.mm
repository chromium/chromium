// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_full_card_requester.h"

#import <vector>

#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/payments/credit_card_access_manager.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/full_card_request_result_delegate_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

using autofill::CreditCard::RecordType::kVirtualCard;

namespace autofill {
class CreditCard;
}  // namespace autofill

@interface ManualFillFullCardRequester ()

// The ProfileIOS instance passed to the initializer.
@property(nonatomic, readonly) ProfileIOS* profile;

// The WebStateList for this instance. Used to instantiate the child
// coordinators lazily.
@property(nonatomic, readonly) WebStateList* webStateList;

@end

@implementation ManualFillFullCardRequester {
  // Obj-C delegate to receive the success or failure result, when
  // asking credit card unlocking.
  __weak id<FullCardRequestResultDelegateObserving> _delegate;
}

- (instancetype)initWithBrowserState:(ProfileIOS*)profile
                        webStateList:(WebStateList*)webStateList
                      resultDelegate:
                          (id<FullCardRequestResultDelegateObserving>)delegate {
  self = [super init];
  if (self) {
    _profile = profile;
    _webStateList = webStateList;
    _delegate = delegate;
  }
  return self;
}

- (void)requestFullCreditCard:(const autofill::CreditCard)card
       withBaseViewController:(UIViewController*)viewController
                   recordType:(autofill::CreditCard::RecordType)recordType
                    fieldType:(manual_fill::PaymentFieldType)fieldType {
  // Payment Request is only enabled in main frame.
  web::WebState* webState = self.webStateList->GetActiveWebState();
  web::WebFramesManager* frames_manager =
      autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          webState);
  web::WebFrame* mainFrame = frames_manager->GetMainWebFrame();
  if (!mainFrame) {
    return;
  }
  autofill::BrowserAutofillManager& autofillManager =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, mainFrame)
          ->GetAutofillManager();

  autofill::CreditCard virtualCard;
  if (recordType == kVirtualCard) {
    virtualCard = autofill::CreditCard::CreateVirtualCard(card);
  }
  autofill::CreditCardAccessManager& creditCardAccessManager =
      autofillManager.GetCreditCardAccessManager();
  __weak __typeof(self) weakSelf = self;
  creditCardAccessManager.FetchCreditCard(
      (recordType == kVirtualCard ? &virtualCard : &card),
      base::BindOnce(^(autofill::CreditCardFetchResult result,
                       const autofill::CreditCard* fetchedCard) {
        [weakSelf onCreditCardFetched:result
                          fetchedCard:fetchedCard
                            fieldType:fieldType];
      }));

  // TODO(crbug.com/40577448): closing CVC requester doesn't restore icon bar
  // above keyboard.
}

#pragma mark - Private methods

// Callback invoked when the card retrieval is finished. It notifies the
// delegate the result of the card retrieval process and provides the card if
// the process succeeded.
- (void)onCreditCardFetched:(autofill::CreditCardFetchResult)result
                fetchedCard:(const autofill::CreditCard*)fetchedCard
                  fieldType:(manual_fill::PaymentFieldType)fieldType {
  if (result == autofill::CreditCardFetchResult::kSuccess && fetchedCard) {
    [_delegate onFullCardRequestSucceeded:*fetchedCard fieldType:fieldType];
  } else {
    [_delegate onFullCardRequestFailed];
  }
}

@end
