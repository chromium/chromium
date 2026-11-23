// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/buttons/toolbar_buttons_mediator.h"

#import "base/callback_list.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/bubble/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_availability.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_web_state_utils.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_menu_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_menu_factory_delegate.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_buttons_consumer.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface ToolbarButtonsMediator () <CRWWebStateObserver,
                                      GeminiBrowserAgentObserving,
                                      PrefObserverDelegate,
                                      ToolbarButtonMenuFactoryDelegate,
                                      WebStateListObserving>
@end

@implementation ToolbarButtonsMediator {
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<ActiveWebStateObservationForwarder>
      _activeWebStateObservationForwarder;
  std::unique_ptr<web::WebStateObserverBridge> _activeWebStateObserver;
  ToolbarButtonMenuFactory* _buttonMenuFactory;
  raw_ptr<PrefService> _prefService;
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<ProfileIOS> _profile;
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  raw_ptr<GeminiBrowserAgent> _geminiBrowserAgent;
  std::unique_ptr<GeminiBrowserAgentObserverBridge> _geminiObserver;
  base::CallbackListSubscription _activeWebStateControlSubscription;
  BOOL _isIncognito;
  // The consumers receiving updates.
  NSHashTable<id<ToolbarButtonsConsumer>>* _activeConsumers;
}

- (instancetype)initWithIncognito:(BOOL)incognito
                     webStateList:(WebStateList*)webStateList
                    actionFactory:(BrowserActionFactory*)actionFactory
                          profile:(ProfileIOS*)profile
                      prefService:(PrefService*)prefService
            authenticationService:(AuthenticationService*)authenticationService
               geminiBrowserAgent:(GeminiBrowserAgent*)geminiBrowserAgent {
  self = [super init];
  if (self) {
    _isIncognito = incognito;
    _profile = profile;
    _activeConsumers = [NSHashTable weakObjectsHashTable];

    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
    _activeWebStateObserver =
        std::make_unique<web::WebStateObserverBridge>(self);
    _activeWebStateObservationForwarder =
        std::make_unique<ActiveWebStateObservationForwarder>(
            webStateList, _activeWebStateObserver.get());
    [self updateActiveWebStateControlObservation:_webStateList
                                                     ->GetActiveWebState()];

    _buttonMenuFactory = [[ToolbarButtonMenuFactory alloc]
        initForToolbarWithIncognito:incognito
                       webStateList:webStateList
                      actionFactory:actionFactory];
    _buttonMenuFactory.delegate = self;

    _authenticationService = authenticationService;
    CHECK(prefService);
    _prefService = prefService;
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(prefService);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        policy::policy_prefs::kIncognitoModeAvailability,
        _prefChangeRegistrar.get());

    _geminiBrowserAgent = geminiBrowserAgent;
    if (_geminiBrowserAgent) {
      _geminiObserver = std::make_unique<GeminiBrowserAgentObserverBridge>(
          self, _geminiBrowserAgent);
    }
  }
  return self;
}

#pragma mark - Public

- (void)addConsumer:(id<ToolbarButtonsConsumer>)consumer {
  CHECK(consumer);
  if ([_activeConsumers containsObject:consumer]) {
    return;
  }
  [_activeConsumers addObject:consumer];
  [self updateConsumers:@[ consumer ]];
}

- (void)removeConsumer:(id<ToolbarButtonsConsumer>)consumer {
  [_activeConsumers removeObject:consumer];
}

- (void)updateConsumer:(id<ToolbarButtonsConsumer>)consumer
          withWebState:(web::WebState*)webState
              animated:(BOOL)animated {
  if (![_activeConsumers containsObject:consumer]) {
    return;
  }
  [self updateConsumers:@[ consumer ] withWebState:webState animated:animated];
}

- (void)disconnect {
  [_activeConsumers removeAllObjects];
  _activeWebStateControlSubscription = {};
  _activeWebStateObservationForwarder.reset();
  _activeWebStateObserver.reset();
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }
  _webStateListObserver.reset();
  _webStateList = nullptr;
  _buttonMenuFactory = nil;
  _navigationBrowserAgent = nullptr;
  _tabBasedIPHAgent = nullptr;
  _geminiObserver.reset();
  _geminiBrowserAgent = nullptr;
  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
  _prefService = nullptr;
  _authenticationService = nullptr;
  _profile = nullptr;
  _delegate = nil;
}

#pragma mark - ToolbarButtonsMutator

- (void)goBack {
  if (!self.navigationBrowserAgent ||
      !self.navigationBrowserAgent->CanGoBack()) {
    return;
  }
  self.navigationBrowserAgent->GoBack();
  if (self.tabBasedIPHAgent) {
    self.tabBasedIPHAgent->NotifyBackForwardButtonTap();
  }
}

- (void)goForward {
  if (!self.navigationBrowserAgent ||
      !self.navigationBrowserAgent->CanGoForward()) {
    return;
  }
  self.navigationBrowserAgent->GoForward();
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

- (void)assistantButtonTapped {
  if (_isIncognito) {
    [self.delegate toolbarButtonsMediatorDidTapAssistantInIncognito:self];
    return;
  }
  if (_geminiBrowserAgent && _geminiBrowserAgent->is_floaty_invoked()) {
    // Gemini floaty already started.
    [self.geminiHandler dismissGeminiFlowWithCompletion:nil];
    return;
  }
  [self.geminiHandler
      startGeminiEntryFlowWithStartupState:
          [[GeminiStartupState alloc]
              initWithEntryPoint:gemini::EntryPoint::Toolbar]
                        baseViewController:self.baseViewController
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
  // The history menu navigates through `NavigationManager` directly, so block
  // it explicitly while an actor controls `activeWebState`.
  if (ActorTabHelper::IsActorControlled(activeWebState)) {
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
  [self updateConsumersWithWebState:webState animated:YES];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self updateConsumersWithWebState:webState animated:YES];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateConsumersWithWebState:webState animated:YES];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self updateConsumersWithWebState:webState animated:YES];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self updateConsumersWithWebState:webState animated:YES];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  [self updateConsumersWithWebState:webState animated:YES];
}

- (void)webStateRealized:(web::WebState*)webState {
  [self updateActiveWebStateControlObservation:webState];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (webStateList->IsBatchInProgress()) {
    return;
  }
  if (status.active_web_state_change()) {
    [self updateActiveWebStateControlObservation:status.new_active_web_state];
    if (status.new_active_web_state) {
      [self updateConsumersWithWebState:status.new_active_web_state
                               animated:NO];
      return;
    }
    for (id<ToolbarButtonsConsumer> consumer in _activeConsumers.allObjects) {
      [consumer setActorControlled:NO];
    }
  }
  [self updateConsumersTabCountAndGroupState:_activeConsumers.allObjects];
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  [self
      updateActiveWebStateControlObservation:webStateList->GetActiveWebState()];
  [self updateConsumers:_activeConsumers.allObjects];
}

#pragma mark - GeminiBrowserAgentObserving

- (void)geminiFloatyInvokedChanged:(BOOL)isInvoked {
  [self updateConsumersAssistantButton:_activeConsumers.allObjects];
}

- (void)geminiAvailabilityChanged:(BOOL)available {
  [self updateConsumersAssistantButton:_activeConsumers.allObjects];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == policy::policy_prefs::kIncognitoModeAvailability) {
    if (_webStateList && _webStateList->GetActiveWebState()) {
      [self updateConsumersWithWebState:_webStateList->GetActiveWebState()
                               animated:NO];
    }
  }
}

#pragma mark - Private

// Updates the active consumers with the state of `webState`.
- (void)updateConsumersWithWebState:(web::WebState*)webState
                           animated:(BOOL)animated {
  [self updateConsumers:_activeConsumers.allObjects
           withWebState:webState
               animated:animated];
}

// Subscribes to actor control state changes on `webState`, which enable or
// disable the navigation buttons of the active consumers.
- (void)updateActiveWebStateControlObservation:(web::WebState*)webState {
  __weak __typeof(self) weakSelf = self;
  _activeWebStateControlSubscription =
      ActorTabHelper::ObserveControlState(webState, base::BindRepeating(^{
                                            [weakSelf updateActiveConsumers];
                                          }));
}

// Syncs the active consumers, if any.
- (void)updateActiveConsumers {
  [self updateConsumers:_activeConsumers.allObjects];
}

// Syncs `consumers` with the active WebState and the WebStateList.
- (void)updateConsumers:(NSArray<id<ToolbarButtonsConsumer>>*)consumers {
  if (!_webStateList) {
    return;
  }
  if (web::WebState* activeWebState = _webStateList->GetActiveWebState()) {
    [self updateConsumers:consumers withWebState:activeWebState animated:NO];
  } else {
    for (id<ToolbarButtonsConsumer> consumer in consumers) {
      [consumer setActorControlled:NO];
    }
    [self updateConsumersTabCountAndGroupState:consumers];
    [self updateConsumersAssistantButton:consumers];
  }
}

// Updates `consumers` with the state of `webState`. The menus are built once
// for all of them, and not at all when there is no consumer.
- (void)updateConsumers:(NSArray<id<ToolbarButtonsConsumer>>*)consumers
           withWebState:(web::WebState*)webState
               animated:(BOOL)animated {
  if (!webState || consumers.count == 0) {
    return;
  }
  BOOL actorControlled =
      !_isIncognito && ActorTabHelper::IsActorControlled(webState);
  BOOL canGoBack = self.navigationBrowserAgent &&
                   self.navigationBrowserAgent->CanGoBack(webState);
  BOOL canGoForward = self.navigationBrowserAgent &&
                      self.navigationBrowserAgent->CanGoForward(webState);
  BOOL shareEnabled = IsUrlShareable(webState);
  BOOL isNtp = IsVisibleURLNewTabPage(webState);
  BOOL isStartSurface = NO;
  if (isNtp) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(webState);
    isStartSurface = NTPHelper && NTPHelper->ShouldShowStartSurface();
  }
  BOOL isLoading = webState->IsLoading();
  double loadingProgress = webState->GetLoadingProgress();
  web::NavigationManager* navigationManager = webState->GetNavigationManager();
  UIMenu* backMenu = [_buttonMenuFactory
      menuForNavigationButton:navigationManager->GetBackwardItems()];
  UIMenu* forwardMenu = [_buttonMenuFactory
      menuForNavigationButton:navigationManager->GetForwardItems()];
  UIMenu* assistantMenu = [_buttonMenuFactory menuForAssistantButton];
  UIMenu* tabGridMenu = [_buttonMenuFactory menuForTabGridButton];

  for (id<ToolbarButtonsConsumer> consumer in consumers) {
    [consumer setActorControlled:actorControlled];
    [consumer setCanGoBack:canGoBack];
    [consumer setCanGoForward:canGoForward animated:animated];
    [consumer setShareEnabled:shareEnabled];
    [consumer setNTPVisible:isNtp
             isStartSurface:isStartSurface
                  isLoading:isLoading
            loadingProgress:loadingProgress];
    [consumer setMenu:backMenu forButtonType:ToolbarButtonTypeBack];
    [consumer setMenu:forwardMenu forButtonType:ToolbarButtonTypeForward];
    [consumer setMenu:assistantMenu forButtonType:ToolbarButtonTypeAssistant];
    [consumer setMenu:tabGridMenu forButtonType:ToolbarButtonTypeTabGrid];
  }
  [self updateConsumersTabCountAndGroupState:consumers];
  [self updateConsumersAssistantButton:consumers];
}

// Updates the tab count and tab group state of `consumers`.
- (void)updateConsumersTabCountAndGroupState:
    (NSArray<id<ToolbarButtonsConsumer>>*)consumers {
  if (consumers.count == 0) {
    return;
  }
  NSUInteger tabCount = 0;
  BOOL inTabGroup = NO;
  if (_webStateList) {
    const TabGroup* group = GetGroupForActiveWebState(_webStateList);
    inTabGroup = group != nullptr;
    tabCount = group ? group->range().count() : _webStateList->count();
  }
  for (id<ToolbarButtonsConsumer> consumer in consumers) {
    [consumer updateTabCount:tabCount];
    [consumer setInTabGroup:inTabGroup];
  }
}

// Updates the assistant button state of `consumers`.
- (void)updateConsumersAssistantButton:
    (NSArray<id<ToolbarButtonsConsumer>>*)consumers {
  if (consumers.count == 0) {
    return;
  }
  web::WebState* activeWebState =
      _webStateList ? _webStateList->GetActiveWebState() : nullptr;
  gemini::GeminiAvailabilityResult result = gemini::IsGeminiAvailable(
      gemini::EntryPoint::Toolbar, _profile, activeWebState,
      _authenticationService, _prefService);
  for (id<ToolbarButtonsConsumer> consumer in consumers) {
    [consumer setAssistantButtonVisible:result.visible enabled:result.enabled];
  }
}

@end
