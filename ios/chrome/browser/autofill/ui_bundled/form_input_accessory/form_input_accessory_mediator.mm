// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/containers/fixed_flat_set.h"
#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/address_data_manager.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_observer_bridge.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/passwords/model/password_counter_delegate_bridge.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/security_alert_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_chromium_text_data.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_suggestion_view.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/scoped_form_input_accessory_reauth_module_override.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::UmaHistogramEnumeration;

namespace {

// Returns whether the input field type triggers the keyboard to open. If the
// field type isn't recognized, it returns the provided default value.
bool InputTriggersKeyboard(std::string field_type, bool default_value) {
  static const auto triggers_keyboard = base::MakeFixedFlatSet<std::string>(
      {"email", "number", "password", "search", "tel", "text", "url", "week"});
  static const auto no_keyboard = base::MakeFixedFlatSet<std::string>(
      {"button", "checkbox", "color", "date", "datetime-local", "file",
       "hidden", "image", "month", "radio", "range", "reset", "submit",
       "time"});

  if (base::Contains(triggers_keyboard, field_type)) {
    return true;
  }

  if (base::Contains(no_keyboard, field_type)) {
    return false;
  }

  return default_value;
}

}  // namespace

@interface FormInputAccessoryMediator () <AutofillBottomSheetObserving,
                                          BooleanObserver,
                                          FormActivityObserver,
                                          FormInputAccessoryViewDelegate,
                                          CRWWebStateObserver,
                                          PasswordCounterObserver,
                                          PersonalDataManagerObserver,
                                          WebStateListObserving>

// The main consumer for this mediator.
@property(nonatomic, weak) id<FormInputAccessoryConsumer> consumer;

// The handler for this object.
@property(nonatomic, weak) id<FormInputAccessoryMediatorHandler> handler;

// The object that manages the currently-shown custom accessory view.
@property(nonatomic, weak) id<FormInputSuggestionsProvider> currentProvider;

// The form input handler. This is in charge of form navigation.
@property(nonatomic, strong)
    FormInputAccessoryViewHandler* formNavigationHandler;

// The object that provides suggestions while filling forms.
@property(nonatomic, weak) id<FormInputSuggestionsProvider> provider;

// YES if the latest form activity was made in a form that supports the
// accessory.
@property(nonatomic, assign) BOOL validActivityForAccessoryView;

// The WebState this instance is observing. Can be null.
@property(nonatomic, assign) web::WebState* webState;

// Reauthentication Module used for re-authentication.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

// Used to present alerts.
@property(nonatomic, weak) id<SecurityAlertCommands> securityAlertHandler;

// ID of the latest query to get suggestions. Using a uint to handle overflow
// which will realistically never happen, but just in case.
@property(nonatomic, assign) uint latestQueryId;

// Feature engagement tracker for notifying promo events.
@property(nonatomic, assign) feature_engagement::Tracker* engagementTracker;

@end

@implementation FormInputAccessoryMediator {
  // The WebStateList this instance is observing in order to update the
  // active WebState.
  raw_ptr<WebStateList> _webStateList;

  // Personal data manager to be observed.
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;

  // C++ to ObjC bridge for PersonalDataManagerObserver.
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge>
      _personalDataManagerObserver;

  // Bridge to observe the web state list from Objective-C.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // The observer for number of passwords in the stores.
  std::unique_ptr<PasswordCounterDelegateBridge> _passwordCounter;

  // Bridge to observe form activity in `_webState`.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  // Bridge for the AutofillBottomSheetObserver.
  std::unique_ptr<autofill::AutofillBottomSheetObserverBridge>
      _autofillBottomSheetObserverBridge;

  // Whether suggestions have previously been shown.
  BOOL _suggestionsHaveBeenShown;

  // The last seen valid params of a form before retrieving suggestions. Or
  // empty if `_hasLastSeenParams` is NO.
  autofill::FormActivityParams _lastSeenParams;

  // If YES `_lastSeenParams` is valid.
  BOOL _hasLastSeenParams;

  // Pref tracking if bottom omnibox is enabled.
  PrefBackedBoolean* _bottomOmniboxEnabled;

  // Whether the keyboard height change notifications are enabled.
  BOOL _keyboardHeightChangeNotificationsEnabled;
}

- (instancetype)
          initWithConsumer:(id<FormInputAccessoryConsumer>)consumer
                   handler:(id<FormInputAccessoryMediatorHandler>)handler
              webStateList:(WebStateList*)webStateList
       personalDataManager:(autofill::PersonalDataManager*)personalDataManager
      profilePasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              profilePasswordStore
      accountPasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              accountPasswordStore
      securityAlertHandler:(id<SecurityAlertCommands>)securityAlertHandler
    reauthenticationModule:(ReauthenticationModule*)reauthenticationModule
         engagementTracker:(feature_engagement::Tracker*)engagementTracker {
  self = [super init];
  if (self) {
    _consumer = consumer;
    _consumer.navigationDelegate = self;
    _handler = handler;
    if (webStateList) {
      _webStateList = webStateList;
      _webStateListObserver =
          std::make_unique<WebStateListObserverBridge>(self);
      _webStateList->AddObserver(_webStateListObserver.get());
      web::WebState* webState = webStateList->GetActiveWebState();
      if (webState) {
        _webState = webState;
        FormSuggestionTabHelper* tabHelper =
            FormSuggestionTabHelper::FromWebState(webState);
        if (tabHelper) {
          _provider = tabHelper->GetAccessoryViewProvider();
        }
        _formActivityObserverBridge =
            std::make_unique<autofill::FormActivityObserverBridge>(_webState,
                                                                   self);
        _autofillBottomSheetObserverBridge =
            std::make_unique<autofill::AutofillBottomSheetObserverBridge>(
                self, AutofillBottomSheetTabHelper::FromWebState(webState));
        _webStateObserverBridge =
            std::make_unique<web::WebStateObserverBridge>(self);
        webState->AddObserver(_webStateObserverBridge.get());
      }
    }
    _formNavigationHandler = [[FormInputAccessoryViewHandler alloc] init];
    _formNavigationHandler.webState = _webState;

    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(applicationDidEnterBackground:)
                          name:UIApplicationDidEnterBackgroundNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(keyboardWillShow:)
                          name:UIKeyboardWillShowNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(keyboardWillChangeFrame:)
                          name:UIKeyboardWillChangeFrameNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(textInputModeDidChange:)
                          name:UITextInputCurrentInputModeDidChangeNotification
                        object:nil];

    // In BVC unit tests the password store doesn't exist. Skip creating the
    // counter.
    // TODO:(crbug.com/878388) Remove this workaround.
    if (profilePasswordStore) {
      _passwordCounter = std::make_unique<PasswordCounterDelegateBridge>(
          self, profilePasswordStore.get(), accountPasswordStore.get());
    }
    if (personalDataManager) {
      _personalDataManager = personalDataManager;
      _personalDataManagerObserver.reset(
          new autofill::PersonalDataManagerObserverBridge(self));
      personalDataManager->AddObserver(_personalDataManagerObserver.get());

      // TODO:(crbug.com/845472) Add earl grey test to verify the credit card
      // button is hidden when local cards are saved and then
      // kAutofillCreditCardEnabled is changed to disabled.
      consumer.creditCardButtonHidden =
          personalDataManager->payments_data_manager().GetCreditCards().empty();

      consumer.addressButtonHidden = personalDataManager->address_data_manager()
                                         .GetProfilesToSuggest()
                                         .empty();
    } else {
      consumer.creditCardButtonHidden = YES;
      consumer.addressButtonHidden = YES;
    }
    _reauthenticationModule = reauthenticationModule;
    _securityAlertHandler = securityAlertHandler;

    // Prevent a flicker from happening by starting with valid activity. This
    // will get updated as soon as a form is interacted.
    _validActivityForAccessoryView = YES;

    _latestQueryId = 0;

    _engagementTracker = engagementTracker;

    self.suggestionsEnabled = YES;

    if (IsBottomOmniboxAvailable()) {
      _bottomOmniboxEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:GetApplicationContext()->GetLocalState()
                     prefName:prefs::kBottomOmnibox];
      [_bottomOmniboxEnabled setObserver:self];
      // Initialize to the current value.
      [self booleanDidChange:_bottomOmniboxEnabled];
    }
  }
  return self;
}

- (void)dealloc {
  // TODO(crbug.com/40272467)
  DUMP_WILL_BE_CHECK(!_formActivityObserverBridge.get());
  DUMP_WILL_BE_CHECK(!_autofillBottomSheetObserverBridge.get());
  DUMP_WILL_BE_CHECK(!_personalDataManager);
  DUMP_WILL_BE_CHECK(!_webState);
  DUMP_WILL_BE_CHECK(!_webStateList);
}

- (void)disconnect {
  _formActivityObserverBridge.reset();
  _autofillBottomSheetObserverBridge.reset();
  if (_personalDataManager && _personalDataManagerObserver.get()) {
    _personalDataManager->RemoveObserver(_personalDataManagerObserver.get());
    _personalDataManagerObserver.reset();
    _personalDataManager = nullptr;
  }
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }
  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;
}

- (void)detachFromWebState {
  [self reset];
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
    _formActivityObserverBridge.reset();
    _autofillBottomSheetObserverBridge.reset();
  }
}

- (BOOL)lastFocusedFieldWasObfuscated {
  return _lastSeenParams.field_type == autofill::kObfuscatedFieldType;
}

- (autofill::FillingProduct)currentProviderMainFillingProduct {
  return self.currentProvider ? self.currentProvider.mainFillingProduct
                              : autofill::FillingProduct::kNone;
}

#pragma mark - KeyboardNotification

- (void)keyboardWillShow:(NSNotification*)notification {
  [self updateSuggestionsIfNeeded];
  _keyboardHeightChangeNotificationsEnabled = YES;
}

- (void)keyboardWillChangeFrame:(NSNotification*)notification {
  if (!_keyboardHeightChangeNotificationsEnabled) {
    return;
  }

  if (@available(iOS 16.1, *)) {
    CGRect oldKeyboardRect =
        [notification.userInfo[UIKeyboardFrameBeginUserInfoKey] CGRectValue];
    CGRect newKeyboardRect =
        [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];

    // We're only interested in quick height only changes.
    if (oldKeyboardRect.origin.x != newKeyboardRect.origin.x ||
        oldKeyboardRect.size.width != newKeyboardRect.size.width) {
      return;
    }

    [self.consumer keyboardHeightChanged:newKeyboardRect.size.height
                               oldHeight:oldKeyboardRect.size.height];
  }
}

- (void)textInputModeDidChange:(NSNotification*)notification {
  // Disable height change notifications when they are caused by the keyboard
  // language or layout changing. They will get re-enabled in the next
  // "keyboardWillShow:" call above.
  _keyboardHeightChangeNotificationsEnabled = NO;
}

#pragma mark - AutofillBottomSheetObserving

- (void)willShowPaymentsBottomSheetWithParams:
    (const autofill::FormActivityParams&)params {
  // Update params in this mediator because -keyboardWillShow will be called
  // before the bottom sheet is being notified to show and that will call
  // -retrieveSuggestionsForForm with the last seen params. Depending on what
  // the current page is auto focused on, it could be the incorrect params and
  // we need to update it.
  _lastSeenParams = params;
  _hasLastSeenParams = YES;
  [self updateSuggestionsIfNeeded];
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);
  self.validActivityForAccessoryView = NO;

  // Return early if `params` is not complete.
  if (params.input_missing) {
    return;
  }

  // Return early if the URL can't be verified.
  std::optional<GURL> pageURL = webState->GetLastCommittedURLIfTrusted();
  if (!pageURL) {
    [self reset];
    return;
  }

  // Return early, pause and reset if the url is not HTML.
  if (!web::UrlHasWebScheme(*pageURL) || !webState->ContentIsHTML()) {
    [self reset];
    return;
  }

  // Return early and reset if frame is missing or can't call JS.
  if (!frame) {
    [self reset];
    return;
  }

  // Return early and reset if element is a picker.
  if (params.field_type == "select-one") {
    [self reset];
    return;
  }

  self.validActivityForAccessoryView = YES;
  NSString* frameID;
  if (frame) {
    frameID = base::SysUTF8ToNSString(frame->GetFrameId());
  }
  DCHECK(frameID.length);

  [self.formNavigationHandler setLastFocusFormActivityWebFrameID:frameID];
  [self synchronizeNavigationControls];

  // Don't look for suggestions in the next events.
  if (params.type == "blur" || params.type == "change" ||
      params.type == "form_changed") {
    return;
  }

  // Check if we need to reload input views after using an input which did not
  // require the keyboard accessory to show up. Err on the side of calling
  // "reloadInputViews" if the input field types are unrecognized.
  if (!InputTriggersKeyboard(_lastSeenParams.field_type,
                             /*default_value=*/false) &&
      InputTriggersKeyboard(params.field_type, /*default_value=*/true)) {
    [GetFirstResponder() reloadInputViews];
  }
  _lastSeenParams = params;
  _hasLastSeenParams = YES;
  [self retrieveSuggestionsForForm:params webState:webState];
}

#pragma mark - FormInputAccessoryViewDelegate

- (void)formInputAccessoryViewDidTapNextButton:(FormInputAccessoryView*)sender {
  [self.formNavigationHandler selectNextElementWithButtonPress];
}

- (void)formInputAccessoryViewDidTapPreviousButton:
    (FormInputAccessoryView*)sender {
  [self.formNavigationHandler selectPreviousElementWithButtonPress];
}

- (void)formInputAccessoryViewDidTapCloseButton:
    (FormInputAccessoryView*)sender {
  [self.formNavigationHandler closeKeyboardWithButtonPress];
}

- (void)formInputAccessoryViewDidTapManualFillButton:
    (FormInputAccessoryView*)sender {
  [self.consumer manualFillButtonPressed:sender.manualFillButton];
}

- (void)formInputAccessoryViewDidTapPasswordManualFillButton:
    (FormInputAccessoryView*)sender {
  [self.consumer
      passwordManualFillButtonPressed:sender.passwordManualFillButton];
}

- (void)formInputAccessoryViewDidTapCreditCardManualFillButton:
    (FormInputAccessoryView*)sender {
  [self.consumer
      creditCardManualFillButtonPressed:sender.creditCardManualFillButton];
}

- (void)formInputAccessoryViewDidTapAddressManualFillButton:
    (FormInputAccessoryView*)sender {
  [self.consumer addressManualFillButtonPressed:sender.addressManualFillButton];
}

- (FormInputAccessoryViewTextData*)textDataforFormInputAccessoryView:
    (FormInputAccessoryView*)sender {
  return ChromiumAccessoryViewTextData();
}

- (void)fromInputAccessoryViewDidTapOmniboxTypingShield:
    (FormInputAccessoryView*)sender {
  [self.formNavigationHandler closeKeyboardWithOmniboxTypingShield];
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasShown:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateSuggestionsIfNeeded];
}

- (void)webStateWasHidden:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self reset];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self reset];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self detachFromWebState];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change()) {
    [self reset];
    [self updateWithNewWebState:status.new_active_web_state];
  }
}

#pragma mark - Public

- (void)setSuggestionsEnabled:(BOOL)enabled {
  // Disable height change notifications when they are caused by the keyboard
  // getting reset. They will get re-enabled in the next "keyboardWillShow:"
  // call above.
  _keyboardHeightChangeNotificationsEnabled = NO;

  _suggestionsEnabled = enabled;
  if (enabled) {
    [self updateSuggestionsIfNeeded];
  }
}

- (BOOL)isInputAccessoryViewActive {
  // Return early if there is no WebState.
  if (!_webState) {
    return NO;
  }

  // Return early if the URL can't be verified.
  std::optional<GURL> pageURL = _webState->GetLastCommittedURLIfTrusted();
  if (!pageURL) {
    return NO;
  }

  // Return early if the url is not HTML.
  if (!web::UrlHasWebScheme(*pageURL) || !_webState->ContentIsHTML()) {
    return NO;
  }

  return self.validActivityForAccessoryView;
}

#pragma mark - Setters

- (void)setCurrentProvider:(id<FormInputSuggestionsProvider>)currentProvider {
  if (_currentProvider == currentProvider) {
    return;
  }
  [_currentProvider inputAccessoryViewControllerDidReset];
  _currentProvider = currentProvider;
  _currentProvider.formInputNavigator = self.formNavigationHandler;
}

#pragma mark - Private

// Returns the reauthentication module, which can be an override for testing
// purposes.
- (ReauthenticationModule*)reauthenticationModule {
  id<ReauthenticationProtocol> overrideModule =
      ScopedFormInputAccessoryReauthModuleOverride::Get();
  return overrideModule ? overrideModule : _reauthenticationModule;
}

- (void)updateSuggestionsIfNeeded {
  if (_hasLastSeenParams && _webState) {
    [self retrieveSuggestionsForForm:_lastSeenParams webState:_webState];
  }
}

// Update the status of the consumer form navigation buttons to match the
// handler state.
- (void)synchronizeNavigationControls {
  __weak __typeof(self) weakSelf = self;
  [self.formNavigationHandler
      fetchPreviousAndNextElementsPresenceWithCompletionHandler:^(
          bool previousButtonEnabled, bool nextButtonEnabled) {
        weakSelf.consumer.formInputNextButtonEnabled = nextButtonEnabled;
        weakSelf.consumer.formInputPreviousButtonEnabled =
            previousButtonEnabled;
      }];
}

// Updates the accessory mediator with the passed web state, its JS suggestion
// manager and the registered provider. If nullptr is passed it will instead
// clear those properties in the mediator.
- (void)updateWithNewWebState:(web::WebState*)webState {
  [self detachFromWebState];
  if (webState) {
    self.webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    webState->AddObserver(_webStateObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(webState, self);
    _autofillBottomSheetObserverBridge =
        std::make_unique<autofill::AutofillBottomSheetObserverBridge>(
            self, AutofillBottomSheetTabHelper::FromWebState(webState));

    FormSuggestionTabHelper* tabHelper =
        FormSuggestionTabHelper::FromWebState(webState);
    if (tabHelper) {
      self.provider = tabHelper->GetAccessoryViewProvider();
    }
    _formNavigationHandler.webState = webState;
  } else {
    self.webState = nullptr;
    self.provider = nil;
  }
}

// Resets the current provider, the consumer view and the navigation handler. As
// well as reenables suggestions.
- (void)reset {
  _lastSeenParams = autofill::FormActivityParams();
  _hasLastSeenParams = NO;
  [self.consumer showAccessorySuggestions:@[]];

  [self.handler resetFormInputView];

  self.suggestionsEnabled = YES;
  self.currentProvider = nil;
}

// Asynchronously queries the providers for an accessory view. Sends it to
// the consumer if found.
- (void)retrieveSuggestionsForForm:(const autofill::FormActivityParams&)params
                          webState:(web::WebState*)webState {
  DCHECK_EQ(webState, self.webState);
  DCHECK(_hasLastSeenParams);

  __weak id<FormInputSuggestionsProvider> weakProvider = self.provider;
  __weak __typeof(self) weakSelf = self;

  // Get the query ID for this query.
  uint queryID = ++_latestQueryId;

  [weakProvider
      retrieveSuggestionsForForm:params
                        webState:self.webState
        accessoryViewUpdateBlock:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormInputSuggestionsProvider> provider) {
          // Ignore suggestions if the results aren't from the latest query
          // which provides the most relevant suggestions to fit the current
          // context (i.e. for the field being focused).
          if (queryID != weakSelf.latestQueryId) {
            return;
          }

          // No suggestions found, return and don't update suggestions in view
          // model.
          if (!suggestions) {
            return;
          }

          [weakSelf updateWithProvider:provider suggestions:suggestions];
        }];
}

// Post the passed `suggestions` to the consumer.
- (void)updateWithProvider:(id<FormInputSuggestionsProvider>)provider
               suggestions:(NSArray<FormSuggestion*>*)suggestions {
  if (!self.suggestionsEnabled) {
    if (self.formInputInteractionDelegate) {
      [self.formInputInteractionDelegate
          focusDidChangedWithFillingProduct:provider.mainFillingProduct];
    }
    return;
  }

  // If suggestions are enabled, update `currentProvider`.
  self.currentProvider = provider;

  // Post it to the consumer.
  self.consumer.mainFillingProduct = provider.mainFillingProduct;
  self.consumer.currentFieldId = _lastSeenParams.field_renderer_id;
  [self.consumer showAccessorySuggestions:suggestions];
  if (suggestions.count) {
    if (provider.type == SuggestionProviderTypeAutofill) {
      default_browser::NotifyAutofillSuggestionsShown(self.engagementTracker);

      if (suggestions.firstObject.featureForIPH !=
          SuggestionFeatureForIPH::kUnknown) {
        [self.handler
            showAutofillSuggestionIPHIfNeededFor:suggestions.firstObject
                                                     .featureForIPH];
      }
    }
  }
}

// Handle applicationDidEnterBackground NSNotification.
- (void)applicationDidEnterBackground:(NSNotification*)notification {
  [self.handler resetFormInputView];
}

// Logs information about what type of suggestion the user selected.
- (void)logReauthenticationEvent:(ReauthenticationEvent)reauthenticationEvent
                            type:(autofill::SuggestionType)type {
  std::string histogramName;
  if (self.currentProvider.type == SuggestionProviderTypePassword) {
    histogramName = "IOS.Reauth.Password.Autofill";
  } else if (self.currentProvider.type == SuggestionProviderTypeAutofill) {
    switch (type) {
      case autofill::SuggestionType::kCreditCardEntry:
      case autofill::SuggestionType::kVirtualCreditCardEntry:
        histogramName = "IOS.Reauth.CreditCard.Autofill";
        break;
      case autofill::SuggestionType::kAddressEntry:
        histogramName = "IOS.Reauth.Address.Autofill";
        break;
      default:
        break;
    }
  }
  if (!histogramName.empty()) {
    UmaHistogramEnumeration(histogramName, reauthenticationEvent);
  }
}

// Handles the selection of a suggestion. `index` indicates the position of the
// suggestion among the available suggestions.
- (void)handleSuggestion:(FormSuggestion*)formSuggestion
                 atIndex:(NSInteger)index {
  if (self.currentProvider.type == SuggestionProviderTypePassword) {
    default_browser::NotifyPasswordAutofillSuggestionUsed(
        self.engagementTracker);
  } else if (formSuggestion.featureForIPH !=
             SuggestionFeatureForIPH::kUnknown) {
    [self.handler
        notifyAutofillSuggestionWithIPHSelectedFor:formSuggestion
                                                       .featureForIPH];
  }
  [self.currentProvider didSelectSuggestion:formSuggestion atIndex:index];
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxEnabled) {
    [self.consumer newOmniboxPositionIsBottom:_bottomOmniboxEnabled.value];
  }
}

#pragma mark - FormSuggestionClient

- (void)didSelectSuggestion:(FormSuggestion*)formSuggestion
                    atIndex:(NSInteger)index {
  [self logReauthenticationEvent:ReauthenticationEvent::kAttempt
                            type:formSuggestion.type];

  if (!formSuggestion.requiresReauth) {
    [self logReauthenticationEvent:ReauthenticationEvent::kSuccess
                              type:formSuggestion.type];
    [self handleSuggestion:formSuggestion atIndex:index];
    return;
  }
  if ([self.reauthenticationModule canAttemptReauth]) {
    NSString* reason = l10n_util::GetNSString(IDS_IOS_AUTOFILL_REAUTH_REASON);
    auto completionHandler = ^(ReauthenticationResult result) {
      if (result != ReauthenticationResult::kFailure) {
        [self logReauthenticationEvent:ReauthenticationEvent::kSuccess
                                  type:formSuggestion.type];
        [self handleSuggestion:formSuggestion atIndex:index];
      } else {
        [self logReauthenticationEvent:ReauthenticationEvent::kFailure
                                  type:formSuggestion.type];
      }
    };

    [self.reauthenticationModule
        attemptReauthWithLocalizedReason:reason
                    canReusePreviousAuth:YES
                                 handler:completionHandler];
  } else {
    [self logReauthenticationEvent:ReauthenticationEvent::kMissingPasscode
                              type:formSuggestion.type];
    [self handleSuggestion:formSuggestion atIndex:index];
  }
}

- (void)didSelectSuggestion:(FormSuggestion*)formSuggestion
                    atIndex:(NSInteger)index
                     params:(const autofill::FormActivityParams&)params {
  CHECK(_lastSeenParams == params);
  [self didSelectSuggestion:formSuggestion atIndex:index];
}

#pragma mark - PasswordCounterObserver

- (void)passwordCounterChanged:(size_t)totalPasswords {
  self.consumer.passwordButtonHidden = !totalPasswords;
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  DCHECK(_personalDataManager);

  self.consumer.creditCardButtonHidden =
      _personalDataManager->payments_data_manager().GetCreditCards().empty();

  self.consumer.addressButtonHidden =
      _personalDataManager->address_data_manager()
          .GetProfilesToSuggest()
          .empty();
}

#pragma mark - Tests

- (void)injectWebState:(web::WebState*)webState {
  [self detachFromWebState];
  _webState = webState;
  if (!_webState) {
    return;
  }
  _webStateObserverBridge = std::make_unique<web::WebStateObserverBridge>(self);
  _webState->AddObserver(_webStateObserverBridge.get());
  _formActivityObserverBridge =
      std::make_unique<autofill::FormActivityObserverBridge>(_webState, self);
}

- (void)injectProvider:(id<FormInputSuggestionsProvider>)provider {
  self.provider = provider;
}

@end
