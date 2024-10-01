// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager_observer.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"

using PaymentsSuggestionBottomSheetExitReason::kBadProvider;

namespace {
// Delay before allowing selecting a suggestion for filling. This helps
// preventing clickjacking by giving more time to the user to understand what
// the bottom sheet does.
base::TimeDelta kSelectSuggestionDelay = base::Milliseconds(500);
}  // namespace

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

  // Information regarding the triggering form for this bottom sheet.
  autofill::FormActivityParams _params;

  // Timestamp recorded when the bottom sheet view did appear which corresponds
  // to when the presentation animation is done. Countdowns start once this
  // timestamp is set with a value.
  std::optional<base::TimeTicks> _viewDidAppearTimestamp;

  // Counter that counts the number of attempts made to fill a suggestion before
  // it actually happened. If the count is bigger than 1 it means that filling a
  // suggestion was rejected at least once for some reason. The most common
  // reason is that filling wasn't allowed yet, to prevent clickjacking.
  int _fillAttemptsCount;
}

#pragma mark - Properties

@synthesize hasCreditCards = _hasCreditCards;

#pragma mark - Initialization

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                              params:(const autofill::FormActivityParams&)params
                 personalDataManager:
                     (autofill::PersonalDataManager*)personalDataManager {
  if ((self = [super init])) {
    _params = params;
    _hasCreditCards = NO;
    _webStateList = webStateList;
    _fillAttemptsCount = 0;

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

    [self setupSuggestionsProvider];
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

- (std::optional<autofill::CreditCard>)creditCardForIdentifier:
    (NSString*)identifier {
  CHECK(identifier);
  CHECK(_personalDataManager);
  const autofill::CreditCard* card =
      _personalDataManager->payments_data_manager().GetCreditCardByGUID(
          base::SysNSStringToUTF8(identifier));
  return card ? std::make_optional(*card) : std::nullopt;
}

- (BOOL)hasCreditCards {
  return _hasCreditCards;
}

- (void)logExitReason:(PaymentsSuggestionBottomSheetExitReason)exitReason {
  base::UmaHistogramEnumeration("IOS.PaymentsBottomSheet.ExitReason",
                                exitReason);
  if (exitReason ==
      PaymentsSuggestionBottomSheetExitReason::kUsePaymentsSuggestion) {
    base::UmaHistogramCounts100("IOS.PaymentsBottomSheet.AcceptAttempts.Accept",
                                _fillAttemptsCount);
  } else {
    base::UmaHistogramCounts100(
        "IOS.PaymentsBottomSheet.AcceptAttempts.Dismiss", _fillAttemptsCount);
  }
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

  const auto& creditCards =
      _personalDataManager->payments_data_manager().GetCreditCardsToSuggest();
  if (creditCards.empty()) {
    [_consumer dismiss];
    return;
  }

  BOOL hasNonLocalCard = NO;
  NSMutableArray<CreditCardData*>* creditCardData =
      [[NSMutableArray alloc] initWithCapacity:creditCards.size()];
  for (const autofill::CreditCard* creditCard : creditCards) {
    CHECK(creditCard);
    // If the current card is enrolled to be a virtual card, create the virtual
    // card and add it to creditCardData array directly before the original
    // card.
    if (creditCard->virtual_card_enrollment_state() ==
        autofill::CreditCard::VirtualCardEnrollmentState::kEnrolled) {
      const autofill::CreditCard virtualCard =
          autofill::CreditCard::CreateVirtualCard(*creditCard);
      [creditCardData
          addObject:[[CreditCardData alloc]
                        initWithCreditCard:virtualCard
                                      icon:[self
                                               iconForCreditCard:creditCard]]];
    }
    [creditCardData
        addObject:[[CreditCardData alloc]
                      initWithCreditCard:*creditCard
                                    icon:[self iconForCreditCard:creditCard]]];
    hasNonLocalCard |= !autofill::IsCreditCardLocal(*creditCard);
  }

  [consumer setCreditCardData:creditCardData showGooglePayLogo:hasNonLocalCard];
  _hasCreditCards = YES;
}

#pragma mark - PaymentsSuggestionBottomSheetDelegate

- (void)didSelectCreditCard:(CreditCardData*)creditCardData
                    atIndex:(NSInteger)index {
  if (!_webStateList) {
    return;
  }

  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }

  FormSuggestionTabHelper* tabHelper =
      FormSuggestionTabHelper::FromWebState(activeWebState);
  DCHECK(tabHelper);

  id<FormInputSuggestionsProvider> provider =
      tabHelper->GetAccessoryViewProvider();
  DCHECK(provider);

  if (provider.type != SuggestionProviderTypeAutofill) {
    // Last resort safety exit: On the unlikely event that the provider was set
    // incorrectly (for example if local predictions and server predictions are
    // different), simply exit and open the keyboard.
    [self disableBottomSheetAndRefocus:YES];
    [self logExitReason:kBadProvider];
    return;
  }
  [self disableBottomSheetAndRefocus:NO];

  // Create a form suggestion containing the selected credit card's backend id
  // so that the suggestion provider can properly fill the form.
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:nil
                               minorValue:nil
                       displayDescription:nil
                                     icon:nil
                                     type:([creditCardData recordType] ==
                                                   autofill::CreditCard::
                                                       RecordType::kVirtualCard
                                               ? autofill::SuggestionType::
                                                     kVirtualCreditCardEntry
                                               : autofill::SuggestionType::
                                                     kCreditCardEntry)
                        backendIdentifier:[creditCardData backendIdentifier]
              fieldByFieldFillingTypeUsed:autofill::EMPTY_TYPE
                           requiresReauth:NO
               acceptanceA11yAnnouncement:
                   base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM))];

  [provider didSelectSuggestion:suggestion atIndex:index params:_params];
}

- (void)disableBottomSheetAndRefocus:(BOOL)refocus {
  if (_webStateList) {
    web::WebState* activeWebState = _webStateList->GetActiveWebState();
    AutofillBottomSheetTabHelper::FromWebState(activeWebState)
        ->DetachPaymentsListenersForAllFrames(refocus);
  }
}

- (void)paymentsBottomSheetViewDidAppear {
  // Start countdown for being able to select suggestions.
  _viewDidAppearTimestamp = base::TimeTicks::Now();
}

- (void)didTapOnPrimaryButton {
  ++_fillAttemptsCount;

  // Allow the action if past the delay for accepting suggestions.
  if (_viewDidAppearTimestamp &&
      base::TimeTicks::Now() - *_viewDidAppearTimestamp >=
          kSelectSuggestionDelay) {
    [self.consumer activatePrimaryButton];
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

// Make sure the suggestions provider is properly set up. We need to make sure
// that FormSuggestionController's "_provider" member is set, which happens
// within [FormSuggestionController onSuggestionsReady:provider:], before the
// credit card suggestion is selected.
// TODO(crbug.com/40929827): Remove this dependency on suggestions.
- (void)setupSuggestionsProvider {
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }

  FormSuggestionTabHelper* tabHelper =
      FormSuggestionTabHelper::FromWebState(activeWebState);
  if (!tabHelper) {
    return;
  }

  id<FormInputSuggestionsProvider> provider =
      tabHelper->GetAccessoryViewProvider();
  // Setting this to true only when we are retrieving suggestions for the bottom
  // sheet. We are not using the results from this call, it is just to set the
  // provider so the bottom sheet can fill the fields later.
  autofill::FormActivityParams params = _params;
  params.has_user_gesture = true;
  [provider retrieveSuggestionsForForm:params
                              webState:activeWebState
              accessoryViewUpdateBlock:nil];
}

// Returns the icon associated with the provided credit card.
- (UIImage*)iconForCreditCard:(const autofill::CreditCard*)creditCard {
  // Check if custom card art is available.
  GURL cardArtURL =
      _personalDataManager->payments_data_manager().GetCardArtURL(*creditCard);
  if (!cardArtURL.is_empty() && cardArtURL.is_valid()) {
    gfx::Image* image = _personalDataManager->payments_data_manager()
                            .GetCreditCardArtImageForUrl(cardArtURL);
    if (image) {
      return image->ToUIImage();
    }
  }

  // Otherwise, try to get the default card icon
  autofill::Suggestion::Icon icon = creditCard->CardIconForAutofillSuggestion();
  return icon == autofill::Suggestion::Icon::kNoIcon
             ? nil
             : ui::ResourceBundle::GetSharedInstance()
                   .GetNativeImageNamed(
                       autofill::CreditCard::IconResourceId(icon))
                   .ToUIImage();
}

@end
