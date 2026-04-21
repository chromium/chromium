// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/toolbar_mediator.h"

#import "base/notimplemented.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_menu_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_menu_factory_delegate.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_consumer.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_height_delegate.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "url/gurl.h"

@interface ToolbarMediator () <BooleanObserver,
                               CRWWebStateObserver,
                               ToolbarButtonMenuFactoryDelegate,
                               WebStateListObserving>
@end

@implementation ToolbarMediator {
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<ActiveWebStateObservationForwarder>
      _activeWebStateObservationForwarder;
  std::unique_ptr<web::WebStateObserverBridge> _activeWebStateObserver;
  ToolbarButtonMenuFactory* _buttonMenuFactory;
  // Pref tracking if bottom omnibox is enabled.
  PrefBackedBoolean* _bottomOmniboxEnabled;
  // Whether this mediator is tracking a toolbar at the top position.
  BOOL _topPosition;
  // The fullscreen controller.
  raw_ptr<FullscreenController> _fullscreenController;
  // Whether the location bar indicator is active.
  BOOL _locationBarIndicatorActive;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                       actionFactory:(BrowserActionFactory*)actionFactory
                fullscreenController:(FullscreenController*)fullscreenController
                         topPosition:(BOOL)topPosition {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    _activeWebStateObserver =
        std::make_unique<web::WebStateObserverBridge>(self);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _activeWebStateObserver.get());

    _buttonMenuFactory = [[ToolbarButtonMenuFactory alloc]
        initForToolbarWithIncognito:_incognito
                       webStateList:_webStateList
                      actionFactory:actionFactory];
    _buttonMenuFactory.delegate = self;

    _fullscreenController = fullscreenController;
    _topPosition = topPosition;
    _locationBarIndicatorActive = NO;

    if (IsBottomOmniboxAvailable()) {
      _bottomOmniboxEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:GetApplicationContext()->GetLocalState()
                     prefName:omnibox::kIsOmniboxInBottomPosition];
      [_bottomOmniboxEnabled setObserver:self];
      // Initialize to the correct value.
      [self booleanDidChange:_bottomOmniboxEnabled];

      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(keyboardWillHide:)
                 name:UIKeyboardWillHideNotification
               object:nil];
      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(keyboardWillShow:)
                 name:UIKeyboardWillShowNotification
               object:nil];
    }
  }
  return self;
}

- (void)updateConsumerWithWebState:(web::WebState*)webState {
  if (!webState) {
    return;
  }
  [self.consumer setCanGoBack:self.navigationBrowserAgent->CanGoBack(webState)];
  [self.consumer
      setCanGoForward:self.navigationBrowserAgent->CanGoForward(webState)];

  const GURL visibleURL = webState->GetVisibleURL();
  [self.consumer setShareEnabled:!visibleURL.is_empty()];

  [self.consumer setNTPVisible:IsUrlNtp(visibleURL)];

  [self.consumer setIsLoading:webState->IsLoading()];
  [self.consumer setLoadingProgress:webState->GetLoadingProgress()];

  [self.consumer
            setMenu:[_buttonMenuFactory
                        menuForNavigationButton:webState->GetNavigationManager()
                                                    ->GetBackwardItems()]
      forButtonType:ToolbarButtonTypeBack];
  [self.consumer
            setMenu:[_buttonMenuFactory
                        menuForNavigationButton:webState->GetNavigationManager()
                                                    ->GetForwardItems()]
      forButtonType:ToolbarButtonTypeForward];
  [self.consumer setMenu:[_buttonMenuFactory menuForAssistantButton]
           forButtonType:ToolbarButtonTypeAssistant];
  /// TODO(crbug.com/493948951): Support context menu for tab grid button in the
  /// Toolbar (iPad).
}

- (void)disconnect {
  _activeWebStateObservationForwarder.reset();
  _activeWebStateObserver.reset();
  _webStateList->RemoveObserver(_webStateListObserver.get());
  _webStateListObserver.reset();
  _webStateList = nullptr;
  _buttonMenuFactory = nil;
  _fullscreenController = nullptr;
}

- (void)setConsumer:(id<ToolbarConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  if (_webStateList) {
    [self updateConsumerWithWebState:_webStateList->GetActiveWebState()];
  }
  [self updateToolbarPosition];
}

#pragma mark - ToolbarMutator

- (void)goBack {
  if (self.navigationBrowserAgent) {
    self.navigationBrowserAgent->GoBack();
  }
}

- (void)goForward {
  if (self.navigationBrowserAgent) {
    self.navigationBrowserAgent->GoForward();
  }
}

- (void)reload {
  if (self.navigationBrowserAgent) {
    self.navigationBrowserAgent->Reload();
  }
}

- (void)stop {
  if (self.navigationBrowserAgent) {
    self.navigationBrowserAgent->StopLoading();
  }
}

#pragma mark - ToolbarButtonMenuFactoryDelegate

- (void)navigateToPageForItem:(web::NavigationItem*)item {
  if (!_webStateList) {
    return;
  }
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }

  int index = activeWebState->GetNavigationManager()->GetIndexOfItem(item);
  DCHECK_NE(index, -1);
  activeWebState->GetNavigationManager()->GoToIndex(index);
}

- (void)createNewTabFromView:(UIView*)sender {
  // Toolbar button menus do not have this functionality.
  NOTREACHED();
}

- (void)createNewTabGroupFromView:(UIView*)sender {
  // Toolbar button menus do not have this functionality.
  NOTREACHED();
}

- (void)addNewTabInCurrentTabGroup {
  // Toolbar button menus do not have this functionality.
  NOTREACHED();
}

- (void)addCurrentTabToGroup:(const TabGroup*)destinationGroup {
  /// TODO(crbug.com/493948951): Implement this (iPad).
  NOTIMPLEMENTED();
}

- (void)removeCurrentTabFromGroup {
  /// TODO(crbug.com/493948951): Implement this (iPad).
  NOTIMPLEMENTED();
}

- (void)moveCurrentTabToGroup:(const TabGroup*)destinationGroup {
  /// TODO(crbug.com/493948951): Implement this (iPad).
  NOTIMPLEMENTED();
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  [self updateConsumerWithWebState:webState];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self updateConsumerWithWebState:webState];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateConsumerWithWebState:webState];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self updateConsumerWithWebState:webState];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self updateConsumerWithWebState:webState];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  [self updateConsumerWithWebState:webState];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change() && status.new_active_web_state) {
    [self updateConsumerWithWebState:status.new_active_web_state];
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxEnabled) {
    [self updateToolbarPosition];
  }
}

#pragma mark - UIKeyboardNotification

- (void)keyboardWillShow:(NSNotification*)notification {
  [self constraintToKeyboard:YES withNotification:notification];
}

- (void)keyboardWillHide:(NSNotification*)notification {
  [self constraintToKeyboard:NO withNotification:notification];
}

#pragma mark - Private

// Updates the position of the toolbar by updating its visibility.
- (void)updateToolbarPosition {
  if (IsBottomOmniboxAvailable()) {
    [self.consumer setVisible:_bottomOmniboxEnabled.value == !_topPosition];
  } else {
    // When the bottom omnibox is not available, only the top toolbar is
    // available.
    [self.consumer setVisible:_topPosition];
  }
}

// Updates keyboard constraints with `notification`. When
// `constraintToKeyboard`, the toolbar is collapsed above the keyboard.
- (void)constraintToKeyboard:(BOOL)shouldConstraintToKeyboard
            withNotification:(NSNotification*)notification {
  if (_topPosition || !_bottomOmniboxEnabled.value) {
    return;
  }
  // Whether to cleanup the location indication previously shown for web
  // content.
  BOOL hideLocationIndicator =
      !shouldConstraintToKeyboard && _locationBarIndicatorActive;

  // Whether to show the secondary toolbar as a location indicator when keyboard
  // is active for web content. Bottom omnibox exclusive.
  BOOL keyboardActiveForWebContent = [self keyboardIsActiveForWebContent];
  BOOL showLocationIndicator = shouldConstraintToKeyboard &&
                               keyboardActiveForWebContent &&
                               !_locationBarIndicatorActive;

  BOOL shouldAnimateOmniboxMovement =
      showLocationIndicator || hideLocationIndicator;
  if (!shouldAnimateOmniboxMovement) {
    return;
  }

  _locationBarIndicatorActive = showLocationIndicator;

  if (showLocationIndicator) {
    if (_fullscreenController) {
      _fullscreenController->EnterForceFullscreenMode(
          /* insets_update_enabled */ false,
          FullscreenModeTransitionTrigger::kForcedByCode);
    }
  } else {
    if (_fullscreenController) {
      _fullscreenController->ExitForceFullscreenMode(
          FullscreenModeTransitionTrigger::kForcedByCode);
    }
  }

  [self.consumer setLocationIndicatorVisible:showLocationIndicator
                             forNotification:notification];
}

// Returns whether the keyboard is active for web content and not interacting
// with the app's UI.
- (BOOL)keyboardIsActiveForWebContent {
  if (_webStateList && _webStateList->GetActiveWebState()) {
    return _webStateList->GetActiveWebState()
        ->GetWebViewProxy()
        .keyboardVisible;
  }
  return NO;
}

@end
