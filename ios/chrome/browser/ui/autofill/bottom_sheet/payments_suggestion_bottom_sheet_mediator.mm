// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager_observer.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
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
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"

// Structure which contains all the required information to display about a
// credit card.
@interface PaymentsSuggestionBottomSheetCreditCardInfo
    : NSObject <PaymentsSuggestionBottomSheetData>

- (instancetype)initWithCreditCard:(const autofill::CreditCard*)creditCard
                              icon:(UIImage*)icon;

@property(nonatomic, strong) NSString* cardNameAndLastFourDigits;
@property(nonatomic, strong) NSString* cardDetails;
@property(nonatomic, strong) NSString* backendIdentifier;
@property(nonatomic, strong) NSString* accessibleCardName;
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
    self.accessibleCardName = [self accessibleCardName:creditCard];
    self.backendIdentifier = base::SysUTF8ToNSString(creditCard->guid());

    if (icon.size.width > 0.0 && icon.size.width < 40.0 && icon.scale > 1.0) {
      // If the icon is smaller than desired, but is scaled, reduce the scale
      // (to a minimum of 1.0) in order to attempt to achieve the desired size.
      self.icon = [UIImage
          imageWithCGImage:[icon CGImage]
                     scale:MAX((icon.scale * icon.size.width / 40.0), 1.0)
               orientation:(icon.imageOrientation)];
    } else {
      self.icon = icon;
    }
  }
  return self;
}

#pragma mark - Private

- (NSString*)accessibleCardName:(const autofill::CreditCard*)creditCard {
  // Get the card name. Prepend the card type if the card name doesn't already
  // start with the card type.
  NSString* cardType = base::SysUTF16ToNSString(
      creditCard->GetRawInfo(autofill::CREDIT_CARD_TYPE));
  NSString* cardAccessibleName =
      base::SysUTF16ToNSString(creditCard->CardNameForAutofillDisplay());
  if (![cardAccessibleName hasPrefix:cardType]) {
    // If the card name doesn't already start with the card type, add the card
    // type at the beginning of the card name.
    cardAccessibleName =
        [@[ cardType, cardAccessibleName ] componentsJoinedByString:@" "];
  }

  // Split the last 4 digits, so that they are pronounced separately. For
  // example, "1215" will become "1 2 1 5" and will read "one two one five"
  // instead of "one thousand two hundred and fifteen".
  NSString* cardLastDigits =
      base::SysUTF16ToNSString(creditCard->LastFourDigits());
  NSMutableArray* digits = [[NSMutableArray alloc] init];
  if (cardLastDigits.length > 0) {
    for (NSUInteger i = 0; i < cardLastDigits.length; i++) {
      [digits addObject:[cardLastDigits substringWithRange:NSMakeRange(i, 1)]];
    }
    cardLastDigits = [digits componentsJoinedByString:@" "];
  }

  // Add mention that the credit card ends with the last 4 digits.
  cardAccessibleName = base::SysUTF16ToNSString(
      l10n_util::GetStringFUTF16(IDS_IOS_PAYMENT_BOTTOM_SHEET_CARD_DESCRIPTION,
                                 base::SysNSStringToUTF16(cardAccessibleName),
                                 base::SysNSStringToUTF16(cardLastDigits)));

  // Either prepend that the card is a virtual card OR append the expiration
  // date.
  if (creditCard->record_type() == autofill::CreditCard::VIRTUAL_CARD) {
    cardAccessibleName = [@[ self.cardDetails, cardAccessibleName ]
        componentsJoinedByString:@" "];
  } else {
    cardAccessibleName = base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CREDIT_CARD_TWO_LINE_LABEL_FROM_NAME,
        base::SysNSStringToUTF16(cardAccessibleName),
        base::SysNSStringToUTF16(self.cardDetails)));
  }

  return cardAccessibleName;
}

@end

@interface PaymentsSuggestionBottomSheetMediator () <
    CRWWebStateObserver,
    PersonalDataManagerObserver,
    WebStateListObserving>

@end

@implementation PaymentsSuggestionBottomSheetMediator {
  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;

  // Bridge and forwarder for observing WebState events. The forwarder is a
  // scoped observation, so the bridge will automatically be removed from the
  // relevant observer list.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<ActiveWebStateObservationForwarder>
      _activeWebStateObservationForwarder;

  // Bridge for observing WebStateList events.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;

  // Personal Data Manager from which we can get Credit Card information.
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;

  // C++ to ObjC bridge for PersonalDataManagerObserver.
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge>
      _personalDataManagerObserver;

  // Scoped observer used to track registration of the
  // PersonalDataManagerObserverBridge.
  std::unique_ptr<
      base::ScopedObservation<autofill::PersonalDataManager,
                              autofill::PersonalDataManagerObserver>>
      _scopedPersonalDataManagerObservation;

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
    if (personalDataManager) {
      _personalDataManager = personalDataManager;
      _personalDataManagerObserver.reset(
          new autofill::PersonalDataManagerObserverBridge(self));
      _scopedPersonalDataManagerObservation = std::make_unique<
          base::ScopedObservation<autofill::PersonalDataManager,
                                  autofill::PersonalDataManagerObserver>>(
          _personalDataManagerObserver.get());
      _scopedPersonalDataManagerObservation->Observe(_personalDataManager);
    }

    // Create and register the observers.
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _webStateObserver.get());
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateListObservation = std::make_unique<
        base::ScopedObservation<WebStateList, WebStateListObserverBridge>>(
        _webStateListObserver.get());
    _webStateListObservation->Observe(_webStateList);
  }
  return self;
}

#pragma mark - Public

- (void)dealloc {
  [self disconnect];
}

- (void)disconnect {
  if (_personalDataManager && _personalDataManagerObserver.get()) {
    _personalDataManager->RemoveObserver(_personalDataManagerObserver.get());
    _personalDataManagerObserver.reset();
  }

  _scopedPersonalDataManagerObservation.reset();

  _webStateListObservation.reset();
  _webStateListObserver.reset();
  _activeWebStateObservationForwarder.reset();
  _webStateObserver.reset();
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

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  DCHECK(_personalDataManager);

  // Refresh the data in the consumer
  if (self.consumer) {
    [self setConsumer:self.consumer];
  }
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (status.active_web_state_change()) {
    [self onWebStateChange];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  DCHECK_EQ(webStateList, _webStateList);
  // `disconnect` cleans up all references to `_webStateList` and objects that
  // depend on it.
  [self disconnect];
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
