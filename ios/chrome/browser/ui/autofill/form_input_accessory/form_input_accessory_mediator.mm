// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_mediator.h"

#include "base/ios/block_types.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#import "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/autofill/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/form_suggestion_view.h"
#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"
#import "ios/chrome/browser/passwords/password_generation_utils.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_consumer.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryMediator () <FormActivityObserver,
                                          FormInputAccessoryViewDelegate,
                                          CRWWebStateObserver,
                                          KeyboardObserverHelperConsumer,
                                          PasswordFetcherDelegate,
                                          PersonalDataManagerObserver,
                                          WebStateListObserving>

// The main consumer for this mediator.
@property(nonatomic, weak) id<FormInputAccessoryConsumer> consumer;

// The delegate for this object.
@property(nonatomic, weak) id<FormInputAccessoryMediatorDelegate> delegate;

// The object that manages the currently-shown custom accessory view.
@property(nonatomic, weak) id<FormInputSuggestionsProvider> currentProvider;

// YES if the first responder is a text input other than the web view.
@property(nonatomic, assign) BOOL editingUIKitTextInput;

// The form input handler. This is in charge of form navigation.
@property(nonatomic, strong)
    FormInputAccessoryViewHandler* formInputAccessoryHandler;

// The JS manager for interacting with the underlying form.
@property(nonatomic, weak) JsSuggestionManager* JSSuggestionManager;

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
               delegate:(id<FormInputAccessoryMediatorDelegate>)delegate
           webStateList:(WebStateList*)webStateList
    personalDataManager:(autofill::PersonalDataManager*)personalDataManager
          passwordStore:
              (scoped_refptr<password_manager::PasswordStore>)passwordStore {
  self = [super init];
  if (self) {
    _consumer = consumer;
    _consumer.navigationDelegate = self;
    _delegate = delegate;
    if (webStateList) {
      _webStateList = webStateList;
      _webStateListObserver =
          std::make_unique<WebStateListObserverBridge>(self);
      _webStateList->AddObserver(_webStateListObserver.get());
      web::WebState* webState = webStateList->GetActiveWebState();
      if (webState) {
        _webState = webState;
        CRWJSInjectionReceiver* injectionReceiver =
            webState->GetJSInjectionReceiver();
        _JSSuggestionManager = base::mac::ObjCCastStrict<JsSuggestionManager>(
            [injectionReceiver instanceOfClass:[JsSuggestionManager class]]);
        [_JSSuggestionManager
            setWebFramesManager:webState->GetWebFramesManager()];
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
    _formInputAccessoryHandler = [[FormInputAccessoryViewHandler alloc] init];
    _formInputAccessoryHandler.JSSuggestionManager = _JSSuggestionManager;

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

#pragma mark - KeyboardObserverHelperConsumer

- (void)keyboardDidStayOnScreen {
  [self.consumer removeAnimationsOnKeyboardView];
}

- (void)keyboardWillChangeToState:(KeyboardState)keyboardState {
  [self updateSuggestionsIfNeeded];
  [self.consumer keyboardWillChangeToState:keyboardState];
  if (!keyboardState.isVisible) {
    [self.delegate mediatorDidDetectKeyboardHide:self];
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

  [self.formInputAccessoryHandler setLastFocusFormActivityWebFrameID:frameID];
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
  [self.formInputAccessoryHandler selectNextElementWithButtonPress];
}

- (void)formInputAccessoryViewDidTapPreviousButton:
    (FormInputAccessoryView*)sender {
  [self.formInputAccessoryHandler selectPreviousElementWithButtonPress];
}

- (void)formInputAccessoryViewDidTapCloseButton:
    (FormInputAccessoryView*)sender {
  [self.formInputAccessoryHandler closeKeyboardWithButtonPress];
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
                     reason:(int)reason {
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
  _currentProvider.formInputNavigator = self.formInputAccessoryHandler;
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

  // Return early if the current text input is not the web view.
  if (self.editingUIKitTextInput) {
    return;
  }

  [self.consumer continueCustomKeyboardView];
}

// Update the status of the consumer form navigation buttons to match the
// handler state.
- (void)synchronizeNavigationControls {
  __weak __typeof(self) weakSelf = self;
  [self.formInputAccessoryHandler
      fetchPreviousAndNextElementsPresenceWithCompletionHandler:^(
          BOOL previousButtonEnabled, BOOL nextButtonEnabled) {
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
    CRWJSInjectionReceiver* injectionReceiver =
        webState->GetJSInjectionReceiver();
    self.JSSuggestionManager = base::mac::ObjCCastStrict<JsSuggestionManager>(
        [injectionReceiver instanceOfClass:[JsSuggestionManager class]]);
    [self.JSSuggestionManager
        setWebFramesManager:webState->GetWebFramesManager()];
    FormSuggestionTabHelper* tabHelper =
        FormSuggestionTabHelper::FromWebState(webState);
    if (tabHelper) {
      self.provider = tabHelper->GetAccessoryViewProvider();
    }
    _formInputAccessoryHandler.JSSuggestionManager = self.JSSuggestionManager;
  } else {
    self.webState = nullptr;
    self.JSSuggestionManager = nil;
    self.provider = nil;
  }
}

// Resets the current provider, the consumer view and the navigation handler. As
// well as reenables suggestions.
- (void)reset {
  _lastSeenParams = autofill::FormActivityParams();
  _hasLastSeenParams = NO;

  [self.consumer restoreOriginalKeyboardView];
  [self.formInputAccessoryHandler reset];

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
    [self.consumer showAccessorySuggestions:suggestions
                           suggestionClient:provider];
  }
}

// Inform the delegate that the app went to the background.
- (void)applicationDidEnterBackground:(NSNotification*)notification {
  [self.delegate mediatorDidDetectMovingToBackground:self];
}

#pragma mark - Keyboard Notifications

// When any text field or text view (e.g. omnibox, settings search bar)
// begins editing, pause the consumer so it doesn't present the custom view over
// the keyboard.
- (void)handleTextInputDidBeginEditing:(NSNotification*)notification {
  self.editingUIKitTextInput = YES;
  [self pauseCustomKeyboardView];
}

// When any text field or text view (e.g. omnibox, settings, card unmask dialog)
// ends editing, continue presenting.
- (void)handleTextInputDidEndEditing:(NSNotification*)notification {
  self.editingUIKitTextInput = NO;
  [self continueCustomKeyboardView];
}

#pragma mark - PasswordFetcherDelegate

- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<autofill::PasswordForm>>)passwords {
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

- (void)injectSuggestionManager:(JsSuggestionManager*)JSSuggestionManager {
  _JSSuggestionManager = JSSuggestionManager;
  _formInputAccessoryHandler.JSSuggestionManager = _JSSuggestionManager;
}

- (void)injectProvider:(id<FormInputSuggestionsProvider>)provider {
  self.provider = provider;
}

@end
