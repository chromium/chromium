// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory_mediator.h"

#include "base/ios/block_types.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#import "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/form_input_accessory_consumer.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_provider.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/form_suggestion_view.h"
#import "ios/chrome/browser/passwords/password_generation_utils.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/keyboard_observer_helper.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/web_frame.h"
#include "ios/web/public/web_state/web_frames_manager.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryMediator ()<FormActivityObserver,
                                         CRWWebStateObserver,
                                         KeyboardObserverHelperDelegate,
                                         WebStateListObserving>

// The JS manager for interacting with the underlying form.
@property(nonatomic, weak) JsSuggestionManager* JSSuggestionManager;

// The main consumer for this mediator.
@property(nonatomic, weak) id<FormInputAccessoryConsumer> consumer;

// The object that manages the currently-shown custom accessory view.
@property(nonatomic, weak) id<FormInputAccessoryViewProvider> currentProvider;

// The form input handler. This is in charge of form navigation.
@property(nonatomic, strong)
    FormInputAccessoryViewHandler* formInputAccessoryHandler;

// The observer to determine when the keyboard dissapears and when it stays.
@property(nonatomic, strong) KeyboardObserverHelper* keyboardObserver;

// Last seen provider. Used to reenable suggestions.
@property(nonatomic, weak) id<FormInputAccessoryViewProvider> lastProvider;

// Last seen suggestions. Used to reenable suggestions.
@property(nonatomic, strong) UIView* lastSuggestionView;

// Whether suggestions are disabled.
@property(nonatomic, assign) BOOL suggestionsDisabled;

// The objects that can provide a custom input accessory view while filling
// forms.
@property(nonatomic, copy)
    NSArray<id<FormInputAccessoryViewProvider>>* providers;

// The WebState this instance is observing. Can be null.
@property(nonatomic, assign) web::WebState* webState;

@end

@implementation FormInputAccessoryMediator {
  // The WebStateList this instance is observing in order to update the
  // active WebState.
  WebStateList* _webStateList;

  // Bridge to observe the web state list from Objective-C.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in |_webState|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  // Whether suggestions have previously been shown.
  BOOL _suggestionsHaveBeenShown;
}

- (instancetype)initWithConsumer:(id<FormInputAccessoryConsumer>)consumer
                    webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _consumer = consumer;
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
            setWebFramesManager:web::WebFramesManager::FromWebState(webState)];

        _providers = @[ FormSuggestionTabHelper::FromWebState(webState)
                            ->GetAccessoryViewProvider() ];
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
                      selector:@selector(handleKeyboardWillShow:)
                          name:UIKeyboardWillShowNotification
                        object:nil];
    _keyboardObserver = [[KeyboardObserverHelper alloc] init];
    _keyboardObserver.delegate = self;
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
    _formActivityObserverBridge.reset();
  }
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }
  _formActivityObserverBridge.reset();
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

#pragma mark - KeyboardObserverHelperDelegate

- (void)keyboardDidStayOnScreen {
  [self.consumer removeAnimationsOnKeyboardView];
}

- (void)keyboardDidHide {
  if (_webState && _webState->IsVisible()) {
    [self reset];
  }
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);
  web::URLVerificationTrustLevel trustLevel;
  const GURL pageURL(webState->GetCurrentURL(&trustLevel));
  if (params.input_missing ||
      trustLevel != web::URLVerificationTrustLevel::kAbsolute ||
      !web::UrlHasWebScheme(pageURL) || !webState->ContentIsHTML()) {
    [self reset];
    return;
  }

  if (autofill::switches::IsAutofillIFrameMessagingEnabled() &&
      (!frame || !frame->CanCallJavaScriptFunction())) {
    [self reset];
    return;
  }

  if (params.type == "blur" || params.type == "change" ||
      params.type == "form_changed") {
    return;
  }

  [_formInputAccessoryHandler
      setLastFocusFormActivityWebFrameID:base::SysUTF8ToNSString(
                                             params.frame_id)];
  [self retrieveAccessoryViewForForm:params webState:webState];
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasShown:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self.consumer continueCustomKeyboardView];
}

- (void)webStateWasHidden:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  // On some iPhone with newers iOS (>11.3) when a view controller is presented,
  // i.e. "all passwords", after dismissing it the keyboard appears and the last
  // element is focused. Different devices were not consistent with minor
  // versions changes. On iPad it always stays dismissed. It is important to
  // reset on iPads because the accessory will stay without the keyboard, due
  // how the it is added, On iPhones it will be hidden and reset when other text
  // element gets the focus. On iPad the keyboard stays dismissed.
  if (IsIPadIdiom()) {
    [self reset];
  } else {
    [self.consumer pauseCustomKeyboardView];
  }
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
  [self updateWithProvider:nil suggestionView:nil];
}

- (void)enableSuggestions {
  self.suggestionsDisabled = NO;
  if (self.lastProvider && self.lastSuggestionView) {
    [self updateWithProvider:self.lastProvider
              suggestionView:self.lastSuggestionView];
  }
}

#pragma mark - Setters

- (void)setCurrentProvider:(id<FormInputAccessoryViewProvider>)currentProvider {
  if (_currentProvider == currentProvider) {
    return;
  }
  [_currentProvider inputAccessoryViewControllerDidReset];
  _currentProvider = currentProvider;
  [_currentProvider setAccessoryViewDelegate:self.formInputAccessoryHandler];
}

#pragma mark - Private

// Updates the accessory mediator with the passed web state, its JS suggestion
// manager and the registered providers. If NULL is passed it will instead clear
// those properties in the mediator.
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
        setWebFramesManager:web::WebFramesManager::FromWebState(webState)];
    self.providers = @[ FormSuggestionTabHelper::FromWebState(webState)
                            ->GetAccessoryViewProvider() ];
    _formInputAccessoryHandler.JSSuggestionManager = self.JSSuggestionManager;
  } else {
    self.webState = NULL;
    self.JSSuggestionManager = nil;
    self.providers = @[];
  }
}

// Resets the current provider, the consumer view and the navigation handler. As
// well as reenables suggestions.
- (void)reset {
  [self.consumer restoreOriginalKeyboardView];
  [self.manualFillAccessoryViewController reset];
  [self.formInputAccessoryHandler reset];

  self.suggestionsDisabled = NO;
  self.currentProvider = nil;
}

// Asynchronously queries the providers for an accessory view. Sends it to
// the consumer if found.
- (void)retrieveAccessoryViewForForm:(const autofill::FormActivityParams&)params
                            webState:(web::WebState*)webState {
  DCHECK_EQ(webState, self.webState);

  // TODO(crbug.com/845472): refactor this overly complex code. There is
  // always at max one provider in _providers.

  // Build a block for each provider that will invoke its completion with YES
  // if the provider can provide an accessory view for the specified form/field
  // and NO otherwise.
  NSMutableArray* findProviderBlocks = [[NSMutableArray alloc] init];
  for (id<FormInputAccessoryViewProvider> provider in _providers) {
    passwords::PipelineBlock findProviderBlock =
        [self queryViewBlockForProvider:provider params:params];
    [findProviderBlocks addObject:findProviderBlock];
  }

  // Run all the blocks in |findProviderBlocks| until one invokes its
  // completion with YES. The first one to do so will be passed to
  // |onProviderFound|.
  passwords::RunSearchPipeline(findProviderBlocks, ^(NSUInteger providerIndex){
                                   // No need to do anything if no suggestions
                                   // are found. The provider will
                                   // update with an empty suggestions array.
                               });
}

// Returns a pipeline block used to search for a provider with the current form
// params.
- (passwords::PipelineBlock)
queryViewBlockForProvider:(id<FormInputAccessoryViewProvider>)provider
                   params:(autofill::FormActivityParams)params {
  __weak __typeof(self) weakSelf = self;
  return ^(void (^completion)(BOOL success)) {
    FormInputAccessoryMediator* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    AccessoryViewReadyCompletion accessoryViewReadyCompletion =
        [self accessoryViewReadyBlockWithCompletion:completion];
    [provider retrieveAccessoryViewForForm:params
                                  webState:strongSelf.webState
                  accessoryViewUpdateBlock:accessoryViewReadyCompletion];
  };
}

// Returns a block setting up the provider and the view returned. It calls the
// passed completion with NO if the view found is invalid. With YES otherwise.
- (AccessoryViewReadyCompletion)accessoryViewReadyBlockWithCompletion:
    (void (^)(BOOL success))completion {
  __weak __typeof(self) weakSelf = self;
  return ^(UIView* accessoryView, id<FormInputAccessoryViewProvider> provider) {
    // View is nil, tell the pipeline to continue searching.
    if (!accessoryView) {
      completion(NO);
      return;
    }
    // Once the view is retrieved, tell the pipeline to stop and
    // update the UI.
    completion(YES);
    [weakSelf updateWithProvider:provider suggestionView:accessoryView];
  };
}

// Post the passed |suggestionView| to the consumer. In case suggestions are
// disabled, it's keep for later.
- (void)updateWithProvider:(id<FormInputAccessoryViewProvider>)provider
            suggestionView:(UIView*)suggestionView {
  // If the povider is valid, save the view and the provider for later. This is
  // used to restore the state when re-enabling suggestions.
  if (provider) {
    self.lastSuggestionView = suggestionView;
    self.lastProvider = provider;
  }
  // If the suggestions are disabled, post this view with no suggestions to the
  // consumer. This allows the navigation buttons be in sync.
  UIView* consumerView = suggestionView;
  if (self.suggestionsDisabled) {
    consumerView = [[FormSuggestionView alloc] initWithFrame:CGRectZero
                                                      client:nil
                                                 suggestions:@[]];
  } else {
    // If suggestions are enabled update |currentProvider|.
    self.currentProvider = provider;
  }
  // If Manual Fallback is enabled, add its view after the suggestions.
  if (autofill::features::IsPasswordManualFallbackEnabled()) {
    FormSuggestionView* formSuggestionView =
        base::mac::ObjCCast<FormSuggestionView>(consumerView);
    formSuggestionView.trailingView =
        self.manualFillAccessoryViewController.view;
  }
  // Post it to the consumer.
  [self.consumer showCustomInputAccessoryView:consumerView
                           navigationDelegate:self.formInputAccessoryHandler];
}

#pragma mark - Keyboard Notifications

// When the keyboard is shown, send the last suggestions to the consumer.
- (void)handleKeyboardWillShow:(NSNotification*)notification {
  if (self.lastSuggestionView) {
    [self updateWithProvider:self.lastProvider
              suggestionView:self.lastSuggestionView];
  }
}

// When any text field or text view (e.g. omnibox, settings search bar)
// begins editing, pause the consumer so it doesn't present the custom view over
// the keyboard.
- (void)handleTextInputDidBeginEditing:(NSNotification*)notification {
  [self.consumer pauseCustomKeyboardView];
}

// When any text field or text view (e.g. omnibox, settings, card unmask dialog)
// ends editing, continue presenting.
- (void)handleTextInputDidEndEditing:(NSNotification*)notification {
  [self.consumer continueCustomKeyboardView];
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

- (void)injectProviders:
    (NSArray<id<FormInputAccessoryViewProvider>>*)providers {
  self.providers = providers;
}

@end
