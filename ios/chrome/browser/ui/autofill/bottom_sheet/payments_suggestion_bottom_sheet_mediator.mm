// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_data.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Structure which contains all the required information to display about a
// credit card.
@interface PaymentsSuggestionBottomSheetCreditCardInfo
    : NSObject <PaymentsSuggestionBottomSheetData>

- (instancetype)initWithCreditCard:(const autofill::CreditCard*)creditCard
                              icon:(UIImage*)icon;

@property(nonatomic, strong) NSString* cardNameAndLastFourDigits;
@property(nonatomic, strong) NSString* cardDetails;
@property(nonatomic, strong) NSString* backendIdentifier;
@property(nonatomic, strong) UIImage* icon;

@end

@implementation PaymentsSuggestionBottomSheetCreditCardInfo

- (instancetype)initWithCreditCard:(const autofill::CreditCard*)creditCard
                              icon:(UIImage*)icon {
  if (self = [super init]) {
    self.cardNameAndLastFourDigits =
        base::SysUTF16ToNSString(creditCard->CardNameAndLastFourDigits());
    self.cardDetails = base::SysUTF16ToNSString(
        (creditCard->record_type() == autofill::CreditCard::VIRTUAL_CARD)
            ? l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE)
            : creditCard->AbbreviatedExpirationDateForDisplay(
                  /* with_prefix=*/false));
    self.backendIdentifier = base::SysUTF8ToNSString(creditCard->guid());
    self.icon = icon;
  }
  return self;
}

@end

@interface PaymentsSuggestionBottomSheetMediator () <CRWWebStateObserver,
                                                     WebStateListObserving>

@end

@implementation PaymentsSuggestionBottomSheetMediator {
  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<web::WebStateObserverBridge> _observer;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;

  // Personal Data Manager from which we can get Credit Card information.
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;

  // Whether the field that triggered the bottom sheet will need to refocus when
  // the bottom sheet is dismissed. Default is true.
  bool _needsRefocus;

  // Information regarding the triggering form for this bottom sheet.
  autofill::FormActivityParams _params;
}

#pragma mark - Properties

@synthesize hasCreditCards = _hasCreditCards;

#pragma mark - Initialization

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                              params:(const autofill::FormActivityParams&)params
                 personalDataManager:
                     (autofill::PersonalDataManager*)personalDataManager {
  if (self = [super init]) {
    _needsRefocus = true;
    _params = params;
    _hasCreditCards = NO;
    _webStateList = webStateList;
    _personalDataManager = personalDataManager;

    // Create and register the observers.
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        webStateList, _observer.get());
  }
  return self;
}

#pragma mark - Public

- (void)disconnect {
  _forwarder = nullptr;
  _observer = nullptr;
  _webStateList = nullptr;
}

- (autofill::CreditCard*)creditCardForIdentifier:(NSString*)identifier {
  CHECK(identifier);
  CHECK(_personalDataManager);
  return _personalDataManager->GetCreditCardByGUID(
      base::SysNSStringToUTF8(identifier));
}

- (BOOL)hasCreditCards {
  return _hasCreditCards;
}

#pragma mark - Accessors

- (void)setConsumer:(id<PaymentsSuggestionBottomSheetConsumer>)consumer {
  _consumer = consumer;

  if (!_consumer) {
    return;
  }

  if (!_personalDataManager) {
    [_consumer dismiss];
    return;
  }

  const auto& creditCards = _personalDataManager->GetCreditCardsToSuggest();
  if (creditCards.empty()) {
    [_consumer dismiss];
    return;
  }

  BOOL hasNonLocalCard = NO;
  NSMutableArray<id<PaymentsSuggestionBottomSheetData>>* creditCardData =
      [[NSMutableArray alloc] initWithCapacity:creditCards.size()];
  for (const auto* creditCard : creditCards) {
    CHECK(creditCard);
    [creditCardData
        addObject:[[PaymentsSuggestionBottomSheetCreditCardInfo alloc]
                      initWithCreditCard:creditCard
                                    icon:[self iconForCreditCard:creditCard]]];
    hasNonLocalCard |= !autofill::IsCreditCardLocal(*creditCard);
  }

  [consumer setCreditCardData:creditCardData showGooglePayLogo:hasNonLocalCard];
  _hasCreditCards = YES;
}

#pragma mark - PaymentsSuggestionBottomSheetDelegate

- (void)didSelectCreditCard:(NSString*)backendIdentifier {
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }

  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  _needsRefocus = false;

  FormSuggestionTabHelper* tabHelper =
      FormSuggestionTabHelper::FromWebState(activeWebState);
  DCHECK(tabHelper);

  id<FormSuggestionClient> client = tabHelper->GetAccessoryViewProvider();
  DCHECK(client);

  // Create a form suggestion containing the selected credit card's backend id
  // so that the suggestion provider can properly fill the form.
  FormSuggestion* suggestion = [FormSuggestion
             suggestionWithValue:nil
              displayDescription:nil
                            icon:nil
                     popupItemId:autofill::PopupItemId::kCreditCardEntry
               backendIdentifier:backendIdentifier
                  requiresReauth:NO
      acceptanceA11yAnnouncement:
          base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM))];

  [client didSelectSuggestion:suggestion params:_params];
}

- (void)disableBottomSheet {
  if (_webStateList) {
    web::WebState* activeWebState = _webStateList->GetActiveWebState();
    AutofillBottomSheetTabHelper::FromWebState(activeWebState)
        ->DetachPaymentsListenersForAllFrames(_needsRefocus);
  }
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // TODO(crbug.com/1442546): Move the implementation from
      // webStateList:didChangeActiveWebState:oldWebState:atIndex:reason to
      // here. Note that here is reachable only when `reason` ==
      // ActiveWebStateChangeReason::Activated.
      break;
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      if (status.index == webStateList->active_index()) {
        [self onWebStateChange];
      }
      break;
    }
    case WebStateListChange::Type::kInsert:
      // Do nothing when a new WebState is inserted.
      break;
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  DCHECK_EQ(_webStateList, webStateList);
  [self onWebStateChange];
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  DCHECK_EQ(webStateList, _webStateList);
  _forwarder = nullptr;
  _observer = nullptr;
  _webStateList = nullptr;
  [self onWebStateChange];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  [self onWebStateChange];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  [self onWebStateChange];
}

#pragma mark - Private

- (void)onWebStateChange {
  [self.consumer dismiss];
}

// Returns the icon associated with the provided credit card.
- (UIImage*)iconForCreditCard:(const autofill::CreditCard*)creditCard {
  // Check if custom card art is available.
  GURL cardArtURL = _personalDataManager->GetCardArtURL(*creditCard);
  if (!cardArtURL.is_empty() && cardArtURL.is_valid()) {
    gfx::Image* image =
        _personalDataManager->GetCreditCardArtImageForUrl(cardArtURL);
    if (image) {
      return image->ToUIImage();
    }
  }

  // Otherwise, try to get the default card icon
  std::string icon = creditCard->CardIconStringForAutofillSuggestion();
  return icon.empty() ? nil
                      : ui::ResourceBundle::GetSharedInstance()
                            .GetNativeImageNamed(
                                autofill::CreditCard::IconResourceId(icon))
                            .ToUIImage();
}

@end
