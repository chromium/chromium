// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_mediator.h"

#include "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/autofill/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/form_suggestion_view.h"
#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"
#import "ios/chrome/browser/passwords/password_generation_utils.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_consumer.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view.h"
#import "ios/chrome/browser/ui/commands/security_alert_commands.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/url_scheme_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UmaHistogramEnumeration;

@interface FormInputAccessoryMediator () <AppStateObserver,
                                          FormActivityObserver,
                                          FormInputAccessoryViewDelegate,
                                          CRWWebStateObserver,
                                          KeyboardObserverHelperConsumer,
                                          PasswordFetcherDelegate,
                                          PersonalDataManagerObserver,
                                          WebStateListObserving>

// The main consumer for this mediator.
@property(nonatomic, weak) id<FormInputAccessoryConsumer> consumer;

// The handler for this object.
@property(nonatomic, weak) id<FormInputAccessoryMediatorHandler> handler;

// The object that manages the currently-shown custom accessory view.
@property(nonatomic, weak) id<FormInputSuggestionsProvider> currentProvider;

// YES if the first responder is valid.
@property(nonatomic, assign) BOOL firstResponderIsValid;

// The form input handler. This is in charge of form navigation.
@property(nonatomic, strong)
    FormInputAccessoryViewHandler* formNavigationHandler;

// The observer to determine when the keyboard dissapears and when it stays.
@property(nonatomic, strong) KeyboardObserverHelper* keyboardObserver;

// The object that provides suggestions while filling forms.
@property(nonatomic, weak) id<FormInputSuggestionsProvider> provider;

// The password fetcher used to know if passwords are available and update the
// consumer accordingly.
@property(nonatomic, strong) PasswordFetcher* passwordFetcher;

// Whether suggestions are disabled.
@property(nonatomic, assign) BOOL suggestionsDisabled;

// YES if the latest form activity was made in a form that supports the
// accessory.
@property(nonatomic, assign) BOOL validActivityForAccessoryView;

// The WebState this instance is observing. Can be null.
@property(nonatomic, assign) web::WebState* webState;

// Contains information about the application state, for example the last window
// that was tapped.
@property(nonatomic, weak) AppState* appState;

// Reauthentication Module used for re-authentication.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

// Used to present alerts.
@property(nonatomic, weak) id<SecurityAlertCommands> securityAlertHandler;

@end

@implementation FormInputAccessoryMediator {
  // The WebStateList this instance is observing in order to update the
  // active WebState.
  WebStateList* _webStateList;

  // Personal data manager to be observed.
  autofill::PersonalDataManager* _personalDataManager;

  // C++ to ObjC bridge for PersonalDataManagerObserver.
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge>
      _personalDataManagerObserver;

  // Bridge to observe the web state list from Objective-C.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in |_webState|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  // Whether suggestions have previously been shown.
  BOOL _suggestionsHaveBeenShown;

  // The last seen valid params of a form before retrieving suggestions. Or
  // empty if |_hasLastSeenParams| is NO.
  autofill::FormActivityParams _lastSeenParams;

  // If YES |_lastSeenParams| is valid.
  BOOL _hasLastSeenParams;
}

- (instancetype)
          initWithConsumer:(id<FormInputAccessoryConsumer>)consumer
                   handler:(id<FormInputAccessoryMediatorHandler>)handler
              webStateList:(WebStateList*)webStateList
       personalDataManager:(autofill::PersonalDataManager*)personalDataManager
             passwordStore:
                 (scoped_refptr<password_manager::PasswordStore>)passwordStore
                  appState:(AppState*)appState
      securityAlertHandler:(id<SecurityAlertCommands>)securityAlertHandler
    reauthenticationModule:(ReauthenticationModule*)reauthenticationModule {
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
        _webStateObserverBridge =
            std::make_unique<web::WebStateObserverBridge>(self);
        webState->AddObserver(_webStateObserverBridge.get());
      }
    }
    _formNavigationHandler = [[FormInputAccessoryViewHandler alloc] init];
    _formNavigationHandler.webState = _webState;

    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(handleTextInputDidBeginEditing:)
                          name:UITextFieldTextDidBeginEditingNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(handleTextInputDidEndEditing:)
                          name:UITextFieldTextDidEndEditingNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(applicationDidEnterBackground:)
                          name:UIApplicationDidEnterBackgroundNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(windowDidBecomeKey:)
                          name:UIWindowDidBecomeKeyNotification
                        object:nil];

    _keyboardObserver = [[KeyboardObserverHelper alloc] init];
    _keyboardObserver.consumer = self;

    // In BVC unit tests the password store doesn't exist. Skip creating the
    // fetcher.
    // TODO:(crbug.com/878388) Remove this workaround.
    if (passwordStore) {
      _passwordFetcher =
          [[PasswordFetcher alloc] initWithPasswordStore:passwordStore
                                                delegate:self
                                                     URL:GURL::EmptyGURL()];
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
          personalDataManager->GetCreditCards().empty();

      consumer.addressButtonHidden =
          personalDataManager->GetProfilesToSuggest().empty();
    } else {
      consumer.creditCardButtonHidden = YES;
      consumer.addressButtonHidden = YES;
    }
    _appState = appState;
    if (!base::ios::IsRunningOnIOS14OrLater()) {
      [_appState addObserver:self];
    }
    _reauthenticationModule = reauthenticationModule;
    _securityAlertHandler = securityAlertHandler;
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (void)disconnect {
  _formActivityObserverBridge.reset();
  if (_personalDataManager && _personalDataManagerObserver.get()) {
    _personalDataManager->RemoveObserver(_personalDataManagerObserver.get());
    _personalDataManagerObserver.reset();
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
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    [_appState removeObserver:self];
  }
}

- (void)detachFromWebState {
  [self reset];
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
    _formActivityObserverBridge.reset();
  }
}

- (BOOL)lastFocusedFieldWasPassword {
  return _lastSeenParams.field_type == autofill::kPasswordFieldType;
}

#pragma mark - KeyboardObserverHelperConsumer

- (void)keyboardDidStayOnScreen {
  [self.consumer removeAnimationsOnKeyboardView];
}

- (void)keyboardWillChangeToState:(KeyboardState)keyboardState {
  if (keyboardState.isVisible) {
    [self verifyFirstResponderAndUpdateCustomKeyboardView];
    [self updateSuggestionsIfNeeded];
  }
  [self.consumer keyboardWillChangeToState:keyboardState];
  if (!keyboardState.isVisible) {
    [self.handler mediatorDidDetectKeyboardHide:self];
  }
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);
  self.validActivityForAccessoryView = NO;

  // Return early if |params| is not complete.
  if (params.input_missing) {
    return;
  }

  // Return early if the URL can't be verified.
  web::URLVerificationTrustLevel trustLevel;
  const GURL pageURL(webState->GetCurrentURL(&trustLevel));
  if (trustLevel != web::URLVerificationTrustLevel::kAbsolute) {
    return;
  }

  // Return early, pause and reset if the url is not HTML.
  if (!web::UrlHasWebScheme(pageURL) || !webState->ContentIsHTML()) {
    [self pauseCustomKeyboardView];
    [self reset];
    return;
  }

  // Return early and reset if frame is missing or can't call JS.
  if (!frame || !frame->CanCallJavaScriptFunction()) {
    [self reset];
    return;
  }

  self.validActivityForAccessoryView = YES;
  [self continueCustomKeyboardView];

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
  _lastSeenParams = params;
  _hasLastSeenParams = YES;
  [self.consumer prepareToShowSuggestions];
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

#pragma mark - CRWWebStateObserver

- (void)webStateWasShown:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self continueCustomKeyboardView];
  [self updateSuggestionsIfNeeded];
}

- (void)webStateWasHidden:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self pauseCustomKeyboardView];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self reset];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self detachFromWebState];
}

#pragma mark - CRWWebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  [self reset];
  [self updateWithNewWebState:newWebState];
}

#pragma mark - Public

- (void)disableSuggestions {
  self.suggestionsDisabled = YES;
}

- (void)enableSuggestions {
  self.suggestionsDisabled = NO;
  [self updateSuggestionsIfNeeded];
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

- (void)updateSuggestionsIfNeeded {
  if (_hasLastSeenParams && _webState) {
    [self retrieveSuggestionsForForm:_lastSeenParams webState:_webState];
  }
}

// Tells the consumer to pause the custom keyboard view.
- (void)pauseCustomKeyboardView {
  [self.consumer pauseCustomKeyboardView];
}

// Tells the consumer to continue the custom keyboard view if the last activity
// is valid, the web state is visible, and there is no other text input.
- (void)continueCustomKeyboardView {
  // Return early if the form is not a supported one.
  if (!self.validActivityForAccessoryView) {
    return;
  }

  // Return early if the current webstate is not visible.
  if (!self.webState || !self.webState->IsVisible()) {
    return;
  }

  // Return early if the current input is not valid.
  if (!self.firstResponderIsValid) {
    return;
  }

  [self.consumer continueCustomKeyboardView];
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

  [self.consumer restoreOriginalKeyboardView];
  [self.formNavigationHandler reset];

  self.suggestionsDisabled = NO;
  self.currentProvider = nil;
}

// Asynchronously queries the providers for an accessory view. Sends it to
// the consumer if found.
- (void)retrieveSuggestionsForForm:(const autofill::FormActivityParams&)params
                          webState:(web::WebState*)webState {
  DCHECK_EQ(webState, self.webState);
  DCHECK(_hasLastSeenParams);

  __weak id<FormInputSuggestionsProvider> provider = self.provider;
  __weak __typeof(self) weakSelf = self;
  [provider
      retrieveSuggestionsForForm:params
                        webState:self.webState
        accessoryViewUpdateBlock:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormInputSuggestionsProvider> provider) {
          // No suggestions found, return.
          if (!suggestions) {
            return;
          }
          [weakSelf updateWithProvider:provider suggestions:suggestions];
        }];
}

// Post the passed |suggestionView| to the consumer. In case suggestions are
// disabled, it's keep for later.
- (void)updateWithProvider:(id<FormInputSuggestionsProvider>)provider
               suggestions:(NSArray<FormSuggestion*>*)suggestions {
  // If the suggestions are disabled, post this view with no suggestions to the
  // consumer. This allows the navigation buttons be in sync.
  if (self.suggestionsDisabled) {
    return;
  } else {
    // If suggestions are enabled update |currentProvider|.
    self.currentProvider = provider;
    // Post it to the consumer.
    [self.consumer showAccessorySuggestions:suggestions];
    if (suggestions.count) {
      if (provider.type == SuggestionProviderTypeAutofill) {
        LogLikelyInterestedDefaultBrowserUserActivity(
            DefaultPromoTypeMadeForIOS);
      }
    }
  }
}

// Handle applicationDidEnterBackground NSNotification.
- (void)applicationDidEnterBackground:(NSNotification*)notification {
  [self.handler mediatorDidDetectMovingToBackground:self];
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  [self verifyFirstResponderAndUpdateCustomKeyboardView];
}

// Verifies that the first responder is a child of WKWebView and that is is not
// a child of SSOSignInViewController. Pause or try to continue the keyboard
// custom view depending on the validity of the first responder.
- (void)verifyFirstResponderAndUpdateCustomKeyboardView {
  if (!self.webState) {
    self.firstResponderIsValid = NO;
    [self pauseCustomKeyboardView];
    return;
  }

  BOOL ancestorIsSSOSignInViewController = NO;
  BOOL ancestorIsWkWebView = NO;

  UIView* webStateContainerView = self.webState->GetView();
  BOOL webStateInKeyWindow = webStateContainerView.window.isKeyWindow;
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    // This is a workaround for a bug in iOS multiwindow, in which you can touch
    // a webView without the window getting the keyboard focus. The result is
    // that you focus a field in the new window gains focus, but keyboard typing
    // continue to happen in the other window.
    // TODO(crbug.com/1109124): Remove this workaround.
    webStateInKeyWindow =
        webStateInKeyWindow &&
        webStateContainerView.window == self.appState.lastTappedWindow;
  }
  if (webStateInKeyWindow) {
    UIResponder* firstResponder = GetFirstResponder();
    while (firstResponder) {
      if ([firstResponder isKindOfClass:NSClassFromString(@"WKWebView")]) {
        ancestorIsWkWebView = YES;
      }
      if ([firstResponder
              isKindOfClass:NSClassFromString(@"SSOSignInViewController")]) {
        ancestorIsSSOSignInViewController = YES;
        break;
      }
      firstResponder = firstResponder.nextResponder;
    }
  }
  self.firstResponderIsValid = webStateInKeyWindow && ancestorIsWkWebView &&
                               !ancestorIsSSOSignInViewController;
  if (self.firstResponderIsValid) {
    [self continueCustomKeyboardView];
  } else {
    [self pauseCustomKeyboardView];
  }
}

#pragma mark - Keyboard Notifications

// When any text field or text view (e.g. omnibox, settings search bar)
// begins editing, pause the consumer so it doesn't present the custom view over
// the keyboard.
- (void)handleTextInputDidBeginEditing:(NSNotification*)notification {
  self.firstResponderIsValid = NO;
  [self pauseCustomKeyboardView];
}

// When any text field or text view (e.g. omnibox, settings, card unmask dialog)
// ends editing, continue presenting.
- (void)handleTextInputDidEndEditing:(NSNotification*)notification {
  [self verifyFirstResponderAndUpdateCustomKeyboardView];
}

#pragma mark - FormSuggestionClient

- (void)didSelectSuggestion:(FormSuggestion*)formSuggestion {
  UmaHistogramEnumeration("IOS.Reauth.Password.Autofill",
                          ReauthenticationEvent::kAttempt);
  __weak __typeof(self) weakSelf = self;
  auto suggestionHandler = ^() {
    if (weakSelf.currentProvider.type == SuggestionProviderTypePassword) {
      LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
    }
    [weakSelf.currentProvider didSelectSuggestion:formSuggestion];
  };

  if (!formSuggestion.requiresReauth) {
    UmaHistogramEnumeration("IOS.Reauth.Password.Autofill",
                            ReauthenticationEvent::kSuccess);
    suggestionHandler();
    return;
  }
  if ([self.reauthenticationModule canAttemptReauth]) {
    NSString* reason = l10n_util::GetNSString(IDS_IOS_AUTOFILL_REAUTH_REASON);
    auto completionHandler = ^(ReauthenticationResult result) {
      if (result != ReauthenticationResult::kFailure) {
        UmaHistogramEnumeration("IOS.Reauth.Password.Autofill",
                                ReauthenticationEvent::kSuccess);
        suggestionHandler();
      } else {
        UmaHistogramEnumeration("IOS.Reauth.Password.Autofill",
                                ReauthenticationEvent::kFailure);
      }
    };

    [self.reauthenticationModule
        attemptReauthWithLocalizedReason:reason
                    canReusePreviousAuth:YES
                                 handler:completionHandler];
  } else {
    UmaHistogramEnumeration("IOS.Reauth.Password.Autofill",
                            ReauthenticationEvent::kMissingPasscode);
    suggestionHandler();
  }
}

#pragma mark - PasswordFetcherDelegate

- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
              passwords {
  self.consumer.passwordButtonHidden = passwords.empty();
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  DCHECK(_personalDataManager);

  self.consumer.creditCardButtonHidden =
      _personalDataManager->GetCreditCards().empty();

  self.consumer.addressButtonHidden =
      _personalDataManager->GetProfilesToSuggest().empty();
}

#pragma mark - AppStateObserver
- (void)appState:(AppState*)appState lastTappedWindowChanged:(UIWindow*)window {
  [self verifyFirstResponderAndUpdateCustomKeyboardView];
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
