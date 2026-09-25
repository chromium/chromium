// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/payments/coordinator/credit_card_suggestion_bottom_sheet_mediator.h"

#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager_observer.h"
#import "components/autofill/core/browser/foundations/autofill_manager.h"
#import "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/payments/ui/credit_card_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"

using PaymentsSuggestionBottomSheetExitReason::kBadProvider;

namespace {

// Returns true if the payments bottom sheet is stateless.
bool IsStateless() {
  return base::FeatureList::IsEnabled(kStatelessFormSuggestionController) &&
         base::FeatureList::IsEnabled(kAutofillPaymentsSheetStateless);
}

// Returns true if the payments bottom sheet is stateless.
bool IsV3() {
  return base::FeatureList::IsEnabled(kAutofillPaymentsSheetV3Ios);
}

// Status of the form fill context for the payments bottom sheet.
enum class FillContextStatus {
  // Context is valid; suggestions may be accepted and filled.
  kValid,
  // Context was invalidated by cross-document navigation, tab detachment, or
  // WebState destruction. Selecting a card must NOT refocus the field.
  kInvalidatedByWebStateChange,
  // Context was invalidated because retrieved suggestions contained no credit
  // cards. Selecting a card dismisses the sheet and refocuses the field.
  kInvalidatedNoSuggestions,
};

}  // namespace

@interface CreditCardSuggestionBottomSheetMediator () <
    CRWWebStateObserver,
    PersonalDataManagerObserver,
    WebStateListObserving>
@end

@implementation CreditCardSuggestionBottomSheetMediator {
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

  // Information regarding the triggering form for this bottom sheet.
  autofill::FormActivityParams _params;

  // Timestamp recorded when the bottom sheet view did appear which corresponds
  // to when the presentation animation is done. Countdowns start once this
  // timestamp is set with a value.
  std::optional<base::TimeTicks> _viewDidAppearTimestamp;

  // Status of the fill context.
  FillContextStatus _fillContextStatus;
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
    // Context deemed valid by default; status quo.
    _fillContextStatus = FillContextStatus::kValid;
    if (personalDataManager) {
      _personalDataManager = personalDataManager;
      _personalDataManagerObserver =
          std::make_unique<autofill::PersonalDataManagerObserverBridge>(
              _personalDataManager, self);
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
  // Clearing the consumer goes through `-setConsumer:`, which is a no-op for a
  // nil consumer; it only drops the reference.
  self.consumer = nil;
  self.delegate = nil;

  _personalDataManagerObserver = nullptr;
  _personalDataManager = nullptr;

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

- (void)logExitReason:(PaymentsSuggestionBottomSheetExitReason)exitReason {
  base::UmaHistogramEnumeration("IOS.PaymentsBottomSheet.ExitReason",
                                exitReason);
}

#pragma mark - Accessors

- (void)setConsumer:(id<CreditCardSuggestionBottomSheetConsumer>)consumer {
  _consumer = consumer;

  if (!_consumer) {
    return;
  }

  if (!_personalDataManager) {
    [_consumer dismiss];
    return;
  }

  const auto& creditCards = autofill::GetCreditCardsToSuggest(
      _personalDataManager->payments_data_manager());
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

#pragma mark - CreditCardSuggestionBottomSheetDelegate

- (void)didSelectCreditCard:(CreditCardData*)creditCardData
                    atIndex:(NSInteger)index {
  // Navigation or WebState change has occurred; the originating frame is
  // detached or destroyed. Suppress refocusing to prevent improperly focusing
  // elements or summoning the keyboard on the new page. No metric is logged
  // as this is not a suggestion provider error.
  if (_fillContextStatus == FillContextStatus::kInvalidatedByWebStateChange) {
    [self disableBottomSheetAndRefocus:NO];
    return;
  }

  web::WebState* activeWebState = [self getActiveWebState];
  if (!activeWebState) {
    return;
  }

  FormSuggestionTabHelper* formSuggestionTabHelper =
      FormSuggestionTabHelper::FromWebState(activeWebState);
  CHECK(formSuggestionTabHelper);

  id<FormInputSuggestionsProvider> crossProvider =
      formSuggestionTabHelper->GetAccessoryViewProvider();
  CHECK(crossProvider);

  // Last resort safety exit: On the unlikely event that the provider was
  // set incorrectly (for example if local predictions and server
  // predictions are different), simply exit and open the keyboard.
  if (IsStateless()) {
    if (_fillContextStatus != FillContextStatus::kValid) {
      [self disableBottomSheetAndRefocus:YES];
      [self logExitReason:kBadProvider];
      return;
    }
  } else if (!IsV3() && crossProvider.type != SuggestionProviderTypeAutofill) {
    [self disableBottomSheetAndRefocus:YES];
    [self logExitReason:kBadProvider];
    return;
  }

  [self fillCreditCard:creditCardData
               atIndex:index
         crossProvider:crossProvider];
}

- (void)fillCreditCard:(CreditCardData*)creditCardData
               atIndex:(NSInteger)index
         crossProvider:(id<FormInputSuggestionsProvider>)crossProvider {
  [self disableBottomSheetAndRefocus:NO];

  // Create a form suggestion containing the selected credit card's backend
  // id so that the suggestion provider can properly fill the form.
  FormSuggestion* suggestion = [FormSuggestion
              suggestionWithValue:nil
                       minorValue:nil
               displayDescription:nil
                             icon:nil
                             type:([creditCardData recordType] ==
                                           autofill::CreditCard::RecordType::
                                               kVirtualCard
                                       ? autofill::SuggestionType::
                                             kVirtualCreditCardEntry
                                       : autofill::SuggestionType::
                                             kCreditCardEntry)
                          payload:autofill::Suggestion::Guid(
                                      base::SysNSStringToUTF8(
                                          [creditCardData backendIdentifier]))
      fieldByFieldFillingTypeUsed:autofill::EMPTY_TYPE
                   requiresReauth:NO
       acceptanceA11yAnnouncement:
           base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
               IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM))];

  // Attach the extra contextual information to the suggestion when using the
  // Stateless sheet.
  if (IsStateless() || IsV3()) {
    AutofillTabHelper* autofillTabHelper = [self autofillTabHelper];
    // At this point we know that there is a valid active webstate so there must
    // be an autofill tab helper. Also, the bottom sheet can only be triggered
    // when this helper exists.
    CHECK(autofillTabHelper);

    // Attach
    id<FormSuggestionProvider> autofillProvider =
        autofillTabHelper->GetSuggestionProvider();
    suggestion = [FormSuggestion copy:suggestion
                         andSetParams:_params
                             provider:autofillProvider];
  }
  [crossProvider didSelectSuggestion:suggestion
                             atIndex:index
                              params:_params
                          completion:nil];
}

- (void)disableBottomSheetAndRefocus:(BOOL)shouldRefocus {
  // Suppress refocusing when the context was invalidated by navigation or
  // WebState change. Callers (such as `viewDidDisappear:`) default to
  // requesting refocus on exit, but if the WebState has navigated to a new
  // document or changed, refocusing would improperly summon the keyboard or
  // focus an element on the new page.
  if (_fillContextStatus == FillContextStatus::kInvalidatedByWebStateChange) {
    shouldRefocus = NO;
  }

  bool useV2 = base::FeatureList::IsEnabled(kAutofillPaymentsSheetV2Ios);
  if (useV2) {
    // Do not remove the listeners for the bottom sheet (aka disable) in V2
    // since the listeners were already removed, as soon as the presentation
    // started. Hence, just refocus if needed.
    if (shouldRefocus) {
      [self refocus];
    }
    return;
  }

  CHECK(!useV2);

  if (AutofillBottomSheetTabHelper* tabHelper = [self tabHelper]) {
    tabHelper->DetachPaymentsListenersForAllFrames(shouldRefocus);
  }
}

- (void)paymentsBottomSheetViewDidAppear {
  // Start countdown for being able to select suggestions.
  _viewDidAppearTimestamp = base::TimeTicks::Now();
}

- (void)didTapOnPrimaryButton {
  // Allow the action if past the delay for accepting suggestions.
  if (_viewDidAppearTimestamp &&
      base::TimeTicks::Now() - *_viewDidAppearTimestamp >=
          autofill_ui_constants::kSelectSuggestionDelay) {
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
  // Dismiss and clean up UI before disconnecting from WebStateList.
  [self onWebStateChange];
  [self disconnect];
}

#pragma mark - CRWWebStateObserver

// Invalidation happens as soon as the navigation starts, before the new
// document can be committed, so that a suggestion retrieved for the previous
// document can never be filled into the next one. The trade-off is that a
// cross-document navigation that ends up being aborted still tears the sheet
// down, which is the safe outcome.
- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  CHECK(navigationContext);
  if (navigationContext->IsSameDocument()) {
    return;
  }

  [self onWebStateChange];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  CHECK(navigationContext);
  if (navigationContext->IsSameDocument() ||
      !navigationContext->HasCommitted()) {
    return;
  }

  [self onWebStateChange];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self onWebStateChange];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  [self onWebStateChange];
}

#pragma mark - Private

// Sets `fillContextStatus`. Acts as a one-way latch: once set to an invalid
// state, it cannot return to `kValid`. `kInvalidatedByWebStateChange` is
// terminal and cannot be overwritten, but takes precedence over
// `kInvalidatedNoSuggestions`.
- (void)setFillContextStatus:(FillContextStatus)fillContextStatus {
  if (_fillContextStatus == FillContextStatus::kInvalidatedByWebStateChange ||
      fillContextStatus == FillContextStatus::kValid) {
    return;
  }
  _fillContextStatus = fillContextStatus;
}

// Handles state changes in the observed WebState (such as cross-document
// navigation, destruction, or tab switching) by invalidating the fill context
// and requesting dismissal of the bottom sheet.
- (void)onWebStateChange {
  [self setFillContextStatus:FillContextStatus::kInvalidatedByWebStateChange];
  [self.delegate
      creditCardSuggestionBottomSheetMediatorDidRequestDismissal:self];
}

// Make sure the suggestions provider is properly set up. We need to make sure
// that FormSuggestionController's "_provider" member is set, which happens
// within [FormSuggestionController onSuggestionsReady:provider:], before the
// credit card suggestion is selected.
// TODO(crbug.com/40929827): Remove this dependency on suggestions.
- (void)setupSuggestionsProvider {
  web::WebState* activeWebState = [self getActiveWebState];
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

  FormSuggestionsReadyCompletion completion = nil;
  if (IsStateless()) {
    // When stateless, use completion to determine if there are any credit card
    // suggestions to determine if the context is still valid for filling CC
    // information.
    __weak __typeof(self) weakSelf = self;
    completion = ^(NSArray<FormSuggestion*>* suggestions,
                   id<FormInputSuggestionsProvider> delegate) {
      [weakSelf onSuggestionsRetrieved:suggestions];
    };
  }

  [provider retrieveSuggestionsForForm:params
                              webState:activeWebState
              accessoryViewUpdateBlock:completion];
}

// Processes retrieved suggestions to determine if the fill context is still
// valid. If no credit card suggestions were returned, invalidates the fill
// context so subsequent selection attempts refocus the form field instead of
// attempting to autofill.
- (void)onSuggestionsRetrieved:(NSArray<FormSuggestion*>*)suggestions {
  if (_fillContextStatus != FillContextStatus::kValid) {
    return;
  }
  BOOL hasCreditCard =
      suggestions.count > 0 &&
      [suggestions indexOfObjectPassingTest:^BOOL(FormSuggestion* suggestion,
                                                  NSUInteger idx, BOOL* stop) {
        return suggestion.type == autofill::SuggestionType::kCreditCardEntry ||
               suggestion.type ==
                   autofill::SuggestionType::kVirtualCreditCardEntry;
      }] != NSNotFound;

  if (!hasCreditCard) {
    [self setFillContextStatus:FillContextStatus::kInvalidatedNoSuggestions];
  }
}

// Returns the icon associated with the provided credit card.
- (UIImage*)iconForCreditCard:(const autofill::CreditCard*)creditCard {
  // Check if custom card art is available.
  GURL cardArtURL =
      _personalDataManager->payments_data_manager().GetCardArtURL(*creditCard);
  if (!cardArtURL.is_empty() && cardArtURL.is_valid()) {
    if (const gfx::Image* const image =
            _personalDataManager->payments_data_manager()
                .GetCachedCardArtImageForUrl(cardArtURL)) {
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

// Returns the AutofillBottomSheetTabHelper for the active webstate or nil if
// it can't be retrieved.
- (AutofillBottomSheetTabHelper*)tabHelper {
  web::WebState* activeWebState = [self getActiveWebState];
  return activeWebState
             ? AutofillBottomSheetTabHelper::FromWebState(activeWebState)
             : nullptr;
}

// Refocuses the field that was blurred to show the payments suggestion
// bottom sheet, if deemded needed.
- (void)refocus {
  if (AutofillBottomSheetTabHelper* tabHelper = [self tabHelper]) {
    tabHelper->RefocusElementIfNeeded(_params.frame_id);
  }
}

// Returns the currently active WebState. Returns nullptr if there is none.
- (web::WebState*)getActiveWebState {
  return _webStateList ? _webStateList->GetActiveWebState() : nullptr;
}

// Returns the AutofillTabHelper for the active webstate or nil if
// it can't be retrieved.
- (AutofillTabHelper*)autofillTabHelper {
  web::WebState* activeWebState = [self getActiveWebState];
  return activeWebState ? AutofillTabHelper::FromWebState(activeWebState)
                        : nullptr;
}

@end
