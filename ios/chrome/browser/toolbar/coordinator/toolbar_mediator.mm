// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/toolbar_mediator.h"

#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/browser_layout_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_consumer.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_height_delegate.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface ToolbarMediator () <BrowserLayoutStateObserver,
                               CRWWebStateObserver,
                               DefaultBrowserBannerAppAgentObserver,
                               WebStateListObserving>
@end

@implementation ToolbarMediator {
  __weak BrowserLayoutState* _browserLayoutState;
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<ActiveWebStateObservationForwarder>
      _activeWebStateObservationForwarder;
  std::unique_ptr<web::WebStateObserverBridge> _activeWebStateObserver;
  // Whether this mediator is tracking a toolbar at the top position.
  BOOL _topPosition;
  // The fullscreen controller.
  raw_ptr<FullscreenController> _fullscreenController;
  // The fullscreen browser agent.
  raw_ptr<FullscreenBrowserAgent> _fullscreenBrowserAgent;
  // Whether the location bar indicator is active.
  BOOL _locationBarIndicatorActive;
  // The default browser banner app agent.
  DefaultBrowserBannerPromoAppAgent* _defaultBrowserBannerAppAgent;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                fullscreenController:(FullscreenController*)fullscreenController
              fullscreenBrowserAgent:
                  (FullscreenBrowserAgent*)fullscreenBrowserAgent
                         topPosition:(BOOL)topPosition
        defaultBrowserBannerAppAgent:
            (DefaultBrowserBannerPromoAppAgent*)defaultBrowserBannerAppAgent {
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

    _fullscreenController = fullscreenController;
    if (IsFullscreenRefactoringEnabled()) {
      _fullscreenBrowserAgent = fullscreenBrowserAgent;
    }
    _topPosition = topPosition;
    _locationBarIndicatorActive = NO;

    if (_topPosition && defaultBrowserBannerAppAgent) {
      _defaultBrowserBannerAppAgent = defaultBrowserBannerAppAgent;
      [_defaultBrowserBannerAppAgent addObserver:self];
    }

    if (IsBottomOmniboxAvailable()) {
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

#pragma mark - Public

- (void)updateConsumerWithWebState:(web::WebState*)webState {
  // While this toolbar has the omnibox, `ToolbarButtonsMediator` updates the
  // NTP visibility with the loading state. Otherwise, the top toolbar still
  // needs it for the tab group indicator and the banner promo.
  if (!webState || [self hasOmnibox] || !_topPosition) {
    return;
  }
  [self.consumer setNTPVisible:IsVisibleURLNewTabPage(webState)
                isStartSurface:NO
                     isLoading:NO
               loadingProgress:0];
}

- (void)disconnect {
  [_browserLayoutState removeObserver:self];
  _browserLayoutState = nil;
  [_defaultBrowserBannerAppAgent removeObserver:self];
  _activeWebStateObservationForwarder.reset();
  _activeWebStateObserver.reset();
  _webStateList->RemoveObserver(_webStateListObserver.get());
  _webStateListObserver.reset();
  _webStateList = nullptr;
  _fullscreenController = nullptr;
  _fullscreenBrowserAgent = nullptr;
  _delegate = nil;
}

- (void)setBrowserLayoutState:(BrowserLayoutState*)browserLayoutState {
  if (_browserLayoutState == browserLayoutState) {
    return;
  }
  [_browserLayoutState removeObserver:self];
  _browserLayoutState = browserLayoutState;
  [_browserLayoutState addObserver:self];
  [self updateToolbarPosition];
  [self updateConsumer];
}

- (BrowserLayoutState*)browserLayoutState {
  return _browserLayoutState;
}

- (void)setConsumer:(id<ToolbarConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self updateToolbarPosition];
  [self updateConsumer];
}

- (void)setUICurrentlySupportsPromo:(BOOL)supports {
  if (_defaultBrowserBannerAppAgent) {
    _defaultBrowserBannerAppAgent.UICurrentlySupportsPromo = supports;
  }
}

#pragma mark - ToolbarMutator

- (void)exitFullscreen {
  FullscreenModeTransitionTrigger trigger =
      FullscreenModeTransitionTrigger::kForcedByUser;

  if (IsFullscreenRefactoringEnabled()) {
    [self.fullscreenCommands exitForceFullscreen];
    [self.fullscreenCommands exitFullscreenWithTrigger:trigger animated:YES];
    return;
  }

  if (_fullscreenController) {
    if (_fullscreenController->IsForceFullscreenMode()) {
      _fullscreenController->ExitForceFullscreenMode(trigger);
    } else {
      _fullscreenController->ExitFullscreen(trigger);
    }
  }
}

- (void)tabGroupIndicatorVisibilityUpdated:(BOOL)visible {
  [self setUICurrentlySupportsPromo:!visible];
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
  if (webStateList->IsBatchInProgress()) {
    return;
  }
  if (status.active_web_state_change() && status.new_active_web_state) {
    [self updateConsumerWithWebState:status.new_active_web_state];
  }
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  [self updateConsumer];
}

#pragma mark - BrowserLayoutStateObserver

- (void)browserLayoutState:(BrowserLayoutState*)browserLayoutState
    didChangeToolbarPosition:(ToolbarPosition)toolbarPosition {
  [self updateToolbarPosition];
  [self updateConsumer];
}

#pragma mark - DefaultBrowserBannerAppAgentObserver

- (void)displayPromoFromAppAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent {
  [self.consumer showBannerPromo];
  if (_fullscreenBrowserAgent) {
    _fullscreenBrowserAgent->InvalidateInsetRange();
  }
}

- (void)hidePromoFromAppAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent {
  [self.consumer hideBannerPromo];
  if (_fullscreenBrowserAgent) {
    _fullscreenBrowserAgent->InvalidateInsetRange();
  }
}

#pragma mark - BannerPromoViewDelegate

- (void)bannerPromoWasTapped:(BannerPromoView*)bannerPromoView {
  [self.settingsHandler
      showDefaultBrowserSettingsFromViewController:nil
                                      sourceForUMA:
                                          DefaultBrowserSettingsPageSource::
                                              kBannerPromo];
  [_defaultBrowserBannerAppAgent promoTapped];
}

- (void)bannerPromoCloseButtonWasTapped:(BannerPromoView*)bannerPromoView {
  [_defaultBrowserBannerAppAgent promoCloseButtonTapped];
}

#pragma mark - UIKeyboardNotification

- (void)keyboardWillShow:(NSNotification*)notification {
  [self constraintToKeyboard:YES withNotification:notification];
}

- (void)keyboardWillHide:(NSNotification*)notification {
  [self constraintToKeyboard:NO withNotification:notification];
}

#pragma mark - Private

// Returns whether this toolbar currently hosts the omnibox.
- (BOOL)hasOmnibox {
  if (!_browserLayoutState) {
    return YES;
  }
  return (_browserLayoutState.toolbarPosition == ToolbarPosition::kTop) ==
         _topPosition;
}

// Updates the position of the toolbar by updating its visibility.
- (void)updateToolbarPosition {
  if (!_browserLayoutState) {
    return;
  }
  BOOL hasOmnibox = [self hasOmnibox];
  if (!hasOmnibox && _locationBarIndicatorActive) {
    [self constraintToKeyboard:NO withNotification:nil];
  }
  [self.consumer setHasOmnibox:hasOmnibox];
  if (self.consumer) {
    [self.delegate toolbarMediator:self didUpdateHasOmnibox:hasOmnibox];
  }
}

// Synchronizes the consumer state with the active WebState and WebStateList.
- (void)updateConsumer {
  if (!_webStateList) {
    return;
  }
  [self updateConsumerWithWebState:_webStateList->GetActiveWebState()];
}

// Updates keyboard constraints with `notification`. When
// `constraintToKeyboard`, the toolbar is collapsed above the keyboard.
- (void)constraintToKeyboard:(BOOL)shouldConstraintToKeyboard
            withNotification:(NSNotification*)notification {
  if (_topPosition) {
    return;
  }

  // Determine if the toolbar should currently be pinned above the keyboard.
  BOOL targetIndicatorActive =
      shouldConstraintToKeyboard &&
      _browserLayoutState.toolbarPosition == ToolbarPosition::kBottom &&
      [self keyboardIsActiveForWebContent];

  // Early return if the indicator is inactive and should remain inactive.
  // If active, we continue so the UI consumer can process frame updates.
  if (!targetIndicatorActive && !_locationBarIndicatorActive) {
    return;
  }

  BOOL stateChanged = (targetIndicatorActive != _locationBarIndicatorActive);
  _locationBarIndicatorActive = targetIndicatorActive;

  // Only transition fullscreen modes if the indicator state is actually
  // changing. This prevents continuous frame updates from looping the
  // animation.
  if (stateChanged) {
    if (IsFullscreenRefactoringEnabled()) {
      [self.fullscreenCommands
          forceFullscreen:targetIndicatorActive
                  feature:ForceFullscreenFeature::kKeyboard];
    } else if (_fullscreenController) {
      FullscreenModeTransitionTrigger trigger =
          FullscreenModeTransitionTrigger::kForcedByCode;
      if (targetIndicatorActive) {
        _fullscreenController->EnterForceFullscreenMode(
            /* insets_update_enabled= */ false, trigger);
      } else {
        _fullscreenController->ExitForceFullscreenMode(trigger);
      }
    }
  }

  [self.consumer setLocationIndicatorVisible:targetIndicatorActive
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
