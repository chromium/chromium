// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/toolbar_mediator.h"

#import "base/metrics/user_metrics.h"
#import "base/notimplemented.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/bubble/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_web_state_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
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
                               DefaultBrowserBannerAppAgentObserver,
                               GeminiBrowserAgentObserving,
                               PrefObserverDelegate,
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
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Pref tracking if bottom omnibox is enabled.
  PrefBackedBoolean* _bottomOmniboxEnabled;
  // Whether this mediator is tracking a toolbar at the top position.
  BOOL _topPosition;
  // The fullscreen controller.
  raw_ptr<FullscreenController> _fullscreenController;
  // Whether the location bar indicator is active.
  BOOL _locationBarIndicatorActive;
  // The default browser banner app agent.
  DefaultBrowserBannerPromoAppAgent* _defaultBrowserBannerAppAgent;
  raw_ptr<GeminiService> _geminiService;
  raw_ptr<GeminiBrowserAgent> _geminiBrowserAgent;
  std::unique_ptr<GeminiBrowserAgentObserverBridge> _geminiObserver;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                       actionFactory:(BrowserActionFactory*)actionFactory
                         prefService:(PrefService*)prefService
                fullscreenController:(FullscreenController*)fullscreenController
                         topPosition:(BOOL)topPosition
        defaultBrowserBannerAppAgent:
            (DefaultBrowserBannerPromoAppAgent*)defaultBrowserBannerAppAgent
               authenticationService:
                   (AuthenticationService*)authenticationService
                       geminiService:(GeminiService*)geminiService
                  geminiBrowserAgent:(GeminiBrowserAgent*)geminiBrowserAgent {
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

    CHECK(prefService);
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(prefService);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        policy::policy_prefs::kIncognitoModeAvailability,
        _prefChangeRegistrar.get());

    _fullscreenController = fullscreenController;
    _topPosition = topPosition;
    _locationBarIndicatorActive = NO;

    _geminiService = geminiService;
    _geminiBrowserAgent = geminiBrowserAgent;

    if (_geminiBrowserAgent) {
      _geminiObserver = std::make_unique<GeminiBrowserAgentObserverBridge>(
          self, _geminiBrowserAgent);
    }

    if (_topPosition && defaultBrowserBannerAppAgent) {
      _defaultBrowserBannerAppAgent = defaultBrowserBannerAppAgent;
      [_defaultBrowserBannerAppAgent addObserver:self];
    }

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

#pragma mark - Public

- (void)updateConsumerWithWebState:(web::WebState*)webState
                          animated:(BOOL)animated {
  if (!webState) {
    return;
  }
  [self updateConsumerNavigationButtons:webState animated:animated];

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
  [self.consumer setMenu:[_buttonMenuFactory menuForTabGridButton]
           forButtonType:ToolbarButtonTypeTabGrid];
  [self updateConsumerTabCountAndGroupState];
  [self updateAssistantButton];
}

- (void)disconnect {
  [_defaultBrowserBannerAppAgent removeObserver:self];
  _activeWebStateObservationForwarder.reset();
  _activeWebStateObserver.reset();
  _webStateList->RemoveObserver(_webStateListObserver.get());
  _webStateListObserver.reset();
  _webStateList = nullptr;
  _buttonMenuFactory = nil;
  _fullscreenController = nullptr;
  _geminiObserver.reset();
  _geminiService = nil;
  _geminiBrowserAgent = nil;
  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
}

- (void)setConsumer:(id<ToolbarConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  if (_webStateList) {
    [self updateConsumerWithWebState:_webStateList->GetActiveWebState()
                            animated:NO];
  }
  [self updateToolbarPosition];
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

- (void)goBack {
  if (self.navigationBrowserAgent) {
    self.navigationBrowserAgent->GoBack();
  }
  if (self.tabBasedIPHAgent) {
    self.tabBasedIPHAgent->NotifyBackForwardButtonTap();
  }
}

- (void)goForward {
  if (self.navigationBrowserAgent) {
    self.navigationBrowserAgent->GoForward();
  }
  if (self.tabBasedIPHAgent) {
    self.tabBasedIPHAgent->NotifyBackForwardButtonTap();
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

- (void)tabGroupIndicatorVisibilityUpdated:(BOOL)visible {
  [self setUICurrentlySupportsPromo:!visible];
}

- (void)assistantButtonTapped {
  GeminiStartupState* startupState = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::Toolbar];
  [self.geminiHandler
      startGeminiEntryFlowWithStartupState:startupState
                        baseViewController:self.baseViewController
                               accessPoint:signin_metrics::AccessPoint::
                                               kIosGeminiButtonToolbar
                  showSnackbarOnCompletion:YES
                                completion:nil];
}

- (void)recordUserActionsForToolsMenuTapped {
  if (!_webStateList) {
    return;
  }
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    return;
  }

  if (IsUrlNtp(webState->GetVisibleURL())) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShowMenuOnNTP"));
  }
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowMenu"));
  if (IsReaderModeActiveInWebState(webState)) {
    base::RecordAction(
        base::UserMetricsAction("MobileToolbarShowMenuFromReaderMode"));
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

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  [self updateConsumerWithWebState:webState animated:YES];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self updateConsumerWithWebState:webState animated:YES];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateConsumerWithWebState:webState animated:YES];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self updateConsumerWithWebState:webState animated:NO];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self updateConsumerWithWebState:webState animated:YES];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  [self updateConsumerWithWebState:webState animated:YES];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change() && status.new_active_web_state) {
    [self updateConsumerWithWebState:status.new_active_web_state animated:NO];
  } else {
    [self updateConsumerTabCountAndGroupState];
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxEnabled) {
    [self updateToolbarPosition];
  }
}

#pragma mark - DefaultBrowserBannerAppAgentObserver

- (void)displayPromoFromAppAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent {
  [self.consumer showBannerPromo];
}

- (void)hidePromoFromAppAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent {
  [self.consumer hideBannerPromo];
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

#pragma mark - GeminiBrowserAgentObserverBridge

- (void)geminiFloatyInvokedChanged:(BOOL)isInvoked {
  [self updateAssistantButton];
}

- (void)geminiAvailabilityChanged:(BOOL)available {
  [self updateAssistantButton];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == policy::policy_prefs::kIncognitoModeAvailability) {
    if (_webStateList && _webStateList->GetActiveWebState()) {
      [self updateConsumerWithWebState:_webStateList->GetActiveWebState()
                              animated:NO];
    }
  }
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

  // Determine if the toolbar should currently be pinned above the keyboard.
  BOOL targetIndicatorActive =
      shouldConstraintToKeyboard && [self keyboardIsActiveForWebContent];

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
    FullscreenModeTransitionTrigger trigger =
        FullscreenModeTransitionTrigger::kForcedByCode;

    if (IsFullscreenRefactoringEnabled()) {
      if (targetIndicatorActive) {
        [self.fullscreenCommands enterFullscreenWithTrigger:trigger
                                                   animated:YES];
      } else {
        [self.fullscreenCommands exitFullscreenWithTrigger:trigger
                                                  animated:YES];
      }
    } else if (_fullscreenController) {
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

// Updates the consumer navigation arrows (forward, back) states for the given
// `webState`.
- (void)updateConsumerNavigationButtons:(web::WebState*)webState
                               animated:(BOOL)animated {
  if (!webState) {
    return;
  }
  const GURL lastCommittedURL = webState->GetLastCommittedURL();
  BOOL isLastCommittedUrlNtp =
      IsUrlNtp(lastCommittedURL) || lastCommittedURL.is_empty();
  BOOL isToolbarTransitioningToVisible =
      isLastCommittedUrlNtp && !IsUrlNtp(webState->GetVisibleURL());

  BOOL canGoForward = self.navigationBrowserAgent->CanGoForward(webState);
  if (isToolbarTransitioningToVisible) {
    // Navigation buttons will be preloaded before the toolbar appears.
    animated = NO;
    if (webState->GetNavigationManager()->GetPendingItemIndex() == -1) {
      // The Web State is mid-navigation from the NTP to a webpage. Prevents the
      // forward button from appearing during the navigation if it will not be
      // present after the navigation.
      canGoForward = NO;
    }
  }

  [self.consumer setCanGoBack:self.navigationBrowserAgent->CanGoBack(webState)];
  [self.consumer setCanGoForward:canGoForward animated:animated];
}

// Updates the consumer tab state.
- (void)updateConsumerTabCountAndGroupState {
  if (_webStateList) {
    const TabGroup* group = GetGroupForActiveWebState(_webStateList);
    if (group) {
      [self.consumer updateTabCount:group->range().count()];
      [self.consumer setInTabGroup:YES];
    } else {
      [self.consumer updateTabCount:_webStateList->count()];
      [self.consumer setInTabGroup:NO];
    }
  } else {
    [self.consumer updateTabCount:0];
    [self.consumer setInTabGroup:NO];
  }
}

// Updates the consumer with the latest assistant button state.
- (void)updateAssistantButton {
  BOOL visible = _geminiBrowserAgent &&
                 _geminiBrowserAgent->IsGeminiAvailableForActiveWebState();
  BOOL enabled = visible;

  [self.consumer setAssistantButtonVisible:visible enabled:enabled];
}

@end
