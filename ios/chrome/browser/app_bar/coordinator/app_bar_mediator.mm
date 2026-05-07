// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>
#import <set>

#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_menu_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_menu_factory_delegate.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

@interface AppBarMediator () <IncognitoStateObserver,
                              SearchEngineObserving,
                              TabGridStateObserver,
                              ToolbarButtonMenuFactoryDelegate,
                              WebStateListObserving>

// The web state list currently observed by this mediator.
@property(nonatomic, assign) WebStateList* currentWebStateList;

// The TabGroup currently visible.
@property(nonatomic, assign) const TabGroup* currentTabGroup;

@end

@implementation AppBarMediator {
  std::unique_ptr<WebStateListObserverBridge> _observerBridge;
  raw_ptr<WebStateList> _regularWebStateList;
  raw_ptr<WebStateList> _incognitoWebStateList;
  raw_ptr<FullscreenController> _regularFullscreenController;
  std::unique_ptr<FullscreenUIUpdater> _regularFullscreenUIUpdater;
  raw_ptr<FullscreenController> _incognitoFullscreenController;
  std::unique_ptr<FullscreenUIUpdater> _incognitoFullscreenUIUpdater;
  raw_ptr<FullscreenBrowserAgent> _regularFullscreenBrowserAgent;
  std::unique_ptr<FullscreenBrowserAgentObserverBridge>
      _regularFullscreenObserver;
  raw_ptr<FullscreenBrowserAgent> _incognitoFullscreenBrowserAgent;
  std::unique_ptr<FullscreenBrowserAgentObserverBridge>
      _incognitoFullscreenObserver;
  raw_ptr<PrefService> _prefService;
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<GeminiService> _geminiService;
  raw_ptr<UrlLoadingBrowserAgent> _URLLoader;
  raw_ptr<TemplateURLService> _templateURLService;
  // Observer for the TemplateURLService.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  TabGridPage _currentPage;
  TabGridState* _tabGridState;
  IncognitoState* _incognitoState;
  ToolbarButtonMenuFactory* _regularButtonMenuFactory;
  ToolbarButtonMenuFactory* _incognitoButtonMenuFactory;
}

- (instancetype)
        initWithRegularWebStateList:(WebStateList*)regularWebStateList
              incognitoWebStateList:(WebStateList*)incognitoWebStateList
        regularFullscreenController:
            (FullscreenController*)regularFullscreenController
      incognitoFullscreenController:
          (FullscreenController*)incognitoFullscreenController
      regularFullscreenBrowserAgent:
          (FullscreenBrowserAgent*)regularFullscreenBrowserAgent
    incognitoFullscreenBrowserAgent:
        (FullscreenBrowserAgent*)incognitoFullscreenBrowserAgent
               regularActionFactory:(BrowserActionFactory*)regularActionFactory
             incognitoActionFactory:
                 (BrowserActionFactory*)incognitoActionFactory
                        prefService:(PrefService*)prefService
                 templateURLService:(TemplateURLService*)templateURLService
              authenticationService:
                  (AuthenticationService*)authenticationService
                      geminiService:(GeminiService*)geminiService
                          URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                       tabGridState:(TabGridState*)tabGridState
                     incognitoState:(IncognitoState*)incognitoState {
  self = [super init];
  if (self) {
    _regularWebStateList = regularWebStateList;
    _incognitoWebStateList = incognitoWebStateList;
    _observerBridge = std::make_unique<WebStateListObserverBridge>(self);

    _regularFullscreenController = regularFullscreenController;
    _incognitoFullscreenController = incognitoFullscreenController;

    _regularFullscreenBrowserAgent = regularFullscreenBrowserAgent;
    _incognitoFullscreenBrowserAgent = incognitoFullscreenBrowserAgent;

    _URLLoader = URLLoader;
    _prefService = prefService;
    _templateURLService = templateURLService;
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);

    _authenticationService = authenticationService;

    _geminiService = geminiService;

    _tabGridState = tabGridState;
    [_tabGridState addObserver:self];

    _incognitoState = incognitoState;
    [_incognitoState addObserver:self];

    _regularButtonMenuFactory = [[ToolbarButtonMenuFactory alloc]
        initForAppBarWithIncognito:NO
                      webStateList:_regularWebStateList
                     actionFactory:regularActionFactory
                templateURLService:_templateURLService
                      tabGridState:_tabGridState];
    _regularButtonMenuFactory.delegate = self;

    _incognitoButtonMenuFactory = [[ToolbarButtonMenuFactory alloc]
        initForAppBarWithIncognito:YES
                      webStateList:_incognitoWebStateList
                     actionFactory:incognitoActionFactory
                templateURLService:_templateURLService
                      tabGridState:_tabGridState];
    _incognitoButtonMenuFactory.delegate = self;

    if (_tabGridState.tabGridVisible) {
      [self updateForTabGridPage:_tabGridState.currentPage];
    } else {
      [self updateForIncognitoVisible:_incognitoState.incognitoContentVisible];
    }
  }
  return self;
}

- (void)setConsumer:
    (id<AppBarConsumer, FullscreenUIElement, FullscreenBrowserAgentObserving>)
        consumer {
  if (consumer == _consumer) {
    return;
  }
  _regularFullscreenUIUpdater.reset();
  _incognitoFullscreenUIUpdater.reset();
  _regularFullscreenObserver.reset();
  _incognitoFullscreenObserver.reset();
  _consumer = consumer;
  if (!_consumer) {
    return;
  }
  if (IsFullscreenRefactoringEnabled()) {
    _regularFullscreenObserver =
        std::make_unique<FullscreenBrowserAgentObserverBridge>(
            _consumer, _regularFullscreenBrowserAgent);
    if (_incognitoFullscreenBrowserAgent) {
      _incognitoFullscreenObserver =
          std::make_unique<FullscreenBrowserAgentObserverBridge>(
              _consumer, _incognitoFullscreenBrowserAgent);
    }
  } else {
    _regularFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        _regularFullscreenController, _consumer);
    if (_incognitoFullscreenController) {
      _incognitoFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
          _incognitoFullscreenController, _consumer);
    }
  }
  [self updateConsumer];
}

- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList {
  _incognitoWebStateList = incognitoWebStateList;
  _incognitoButtonMenuFactory = nil;
  if (_tabGridState.tabGridVisible &&
      _currentPage == TabGridPageIncognitoTabs) {
    self.currentWebStateList = _incognitoWebStateList;
  } else if (_incognitoState.incognitoContentVisible) {
    self.currentWebStateList = _incognitoWebStateList;
  }
}

- (void)setIncognitoFullscreenController:
    (FullscreenController*)fullscreenController {
  _incognitoFullscreenUIUpdater.reset();
  _incognitoFullscreenController = fullscreenController;
  if (_incognitoFullscreenController && _consumer) {
    _incognitoFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        _incognitoFullscreenController, _consumer);
  }
}

- (void)setIncognitoFullscreenBrowserAgent:
    (FullscreenBrowserAgent*)fullscreenBrowserAgent {
  _incognitoFullscreenObserver.reset();
  _incognitoFullscreenBrowserAgent = fullscreenBrowserAgent;
  if (_incognitoFullscreenBrowserAgent && _consumer) {
    _incognitoFullscreenObserver =
        std::make_unique<FullscreenBrowserAgentObserverBridge>(
            _consumer, _incognitoFullscreenBrowserAgent);
  }
}

- (void)setIncognitoActionFactory:
    (BrowserActionFactory*)incognitoActionFactory {
  if (incognitoActionFactory) {
    _incognitoButtonMenuFactory = [[ToolbarButtonMenuFactory alloc]
        initForAppBarWithIncognito:YES
                      webStateList:_incognitoWebStateList
                     actionFactory:incognitoActionFactory
                templateURLService:_templateURLService
                      tabGridState:_tabGridState];
    _incognitoButtonMenuFactory.delegate = self;
  } else {
    _incognitoButtonMenuFactory = nil;
  }
  [self updateConsumer];
}

- (void)disconnect {
  self.consumer = nil;
  _regularButtonMenuFactory = nil;
  _incognitoButtonMenuFactory = nil;
  self.currentTabGroup = nullptr;
  if (self.currentWebStateList) {
    self.currentWebStateList->RemoveObserver(_observerBridge.get());
    self.currentWebStateList = nullptr;
  }
  _regularFullscreenUIUpdater.reset();
  _incognitoFullscreenUIUpdater.reset();
  _regularFullscreenObserver.reset();
  _incognitoFullscreenObserver.reset();
  _regularFullscreenController = nullptr;
  _incognitoFullscreenController = nullptr;
  _regularFullscreenBrowserAgent = nullptr;
  _incognitoFullscreenBrowserAgent = nullptr;
  _regularFullscreenHandler = nil;
  _incognitoFullscreenHandler = nil;
  [_tabGridState removeObserver:self];
  [_incognitoState removeObserver:self];
  _observerBridge.reset();
  _regularWebStateList = nullptr;
  _incognitoWebStateList = nullptr;
  _prefService = nullptr;
  _searchEngineObserver.reset();
  _templateURLService = nullptr;
  _authenticationService = nullptr;
  _geminiService = nullptr;
  _URLLoader = nullptr;
  _incognitoState = nil;
  _tabGridState = nil;
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change() && !_tabGridState.tabGridVisible) {
    self.currentTabGroup = GetGroupForActiveWebState(webStateList);
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
    case WebStateListChange::Type::kMove:
    case WebStateListChange::Type::kReplace:
      // Do nothing when web state count is the same.
      break;
    case WebStateListChange::Type::kDetach:
    case WebStateListChange::Type::kInsert:
      [self updateConsumer];
      break;
    case WebStateListChange::Type::kGroupCreate:
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      break;
    case WebStateListChange::Type::kGroupMove: {
      const WebStateListChangeGroupMove& move =
          change.As<WebStateListChangeGroupMove>();
      if (move.moved_group() == self.currentTabGroup) {
        [self updateConsumer];
      }
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& deletion =
          change.As<WebStateListChangeGroupDelete>();
      if (deletion.deleted_group() == self.currentTabGroup) {
        self.currentTabGroup = nullptr;
      }
      break;
    }
  }
  [self updateConsumer];
}

#pragma mark - IncognitoStateObserver

- (void)willEnterIncognitoForState:(IncognitoState*)incognitoState {
  if (_tabGridState.tabGridVisible) {
    return;
  }
  [self updateButtonsForCurrentTabGridPage];
  [self updateForIncognitoVisible:YES];
}

- (void)willExitIncognitoForState:(IncognitoState*)incognitoState {
  if (_tabGridState.tabGridVisible) {
    return;
  }
  [self updateButtonsForCurrentTabGridPage];
  [self updateForIncognitoVisible:NO];
}

- (void)didUpdateAuthenticationRequirementForState:
    (IncognitoState*)incognitoState {
  CHECK_EQ(incognitoState, _incognitoState);
  [self updateButtonsForCurrentTabGridPage];
}

- (void)didUpdateIncognitoLockStateForState:(IncognitoState*)incognitoState {
  [self didUpdateAuthenticationRequirementForState:incognitoState];
}

#pragma mark - TabGridStateObserver

- (void)willEnterTabGrid {
  _currentPage = _tabGridState.currentPage;
  self.currentTabGroup = _tabGridState.visibleTabGroup;

  if (IsFullscreenRefactoringEnabled()) {
    id<FullscreenCommands> fullscreenHandler =
        _incognitoState.incognitoContentVisible
            ? self.incognitoFullscreenHandler
            : self.regularFullscreenHandler;
    [fullscreenHandler
        exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                         animated:YES];
  } else {
    FullscreenController* fullscreenController =
        _incognitoState.incognitoContentVisible ? _incognitoFullscreenController
                                                : _regularFullscreenController;

    if (fullscreenController && fullscreenController->GetProgress() < 1.0) {
      fullscreenController->ExitFullscreen(
          FullscreenModeTransitionTrigger::kForcedByCode);
    }
  }
  [self updateConsumer];
}

- (void)willExitTabGrid {
  self.currentTabGroup = GetGroupForActiveWebState(self.currentWebStateList);
  [self updateForIncognitoVisible:_incognitoState.incognitoContentVisible];
}

- (void)willChangePageTo:(TabGridPage)page {
  _currentPage = page;
  if (!_tabGridState.tabGridVisible) {
    return;
  }
  [self updateForTabGridPage:page];
}

- (void)willShowTabGroup:(const TabGroup*)group {
  if (!_tabGridState.tabGridVisible) {
    return;
  }
  self.currentTabGroup = group;
}

- (void)willHideTabGroup {
  if (!_tabGridState.tabGridVisible) {
    return;
  }
  self.currentTabGroup = nullptr;
  [self updateForTabGridPage:_tabGridState.currentPage];
}

- (void)tabGridStateModeDidChange:(TabGridState*)tabGridState {
  CHECK_EQ(tabGridState, _tabGridState);
  if (tabGridState.mode == TabGridMode::kSelection ||
      tabGridState.mode == TabGridMode::kSearch) {
    [self.consumer setButtonsEnabled:NO];
  } else {
    [self updateButtonsForCurrentTabGridPage];
  }
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  BOOL incognito = self.currentWebStateList == _incognitoWebStateList;
  ToolbarButtonMenuFactory* buttonMenuFactory =
      incognito ? _incognitoButtonMenuFactory : _regularButtonMenuFactory;

  // Update the long press menu actions to replace lens with QR scanner or vice
  // versa, based on the new default search engine.
  [self.consumer setMenu:[buttonMenuFactory menuForNewTabButton]
           forButtonType:AppBarButtonType::AppBarButtonTypeNewTab];
}

#pragma mark - AppBarMutator

- (void)createNewTabFromView:(UIView*)sender {
  if (_tabGridState.tabGridVisible) {
    switch (_tabGridState.currentPage) {
      case TabGridPageRegularTabs:
      case TabGridPageTabGroups:
        // This is an intentional fallthrough. Tabs created while on the tab
        // group page of the tab grid should be non-incognito.
        [self openNewTabInTabGridInIncognito:NO];
        return;
      case TabGridPageIncognitoTabs:
        [self openNewTabInTabGridInIncognito:YES];
        return;
    }
  } else {
    CGPoint center = [sender.superview convertPoint:sender.center toView:nil];
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithIncognito:_incognitoState.incognitoContentVisible
                 originPoint:center];
    [self.sceneHandler openURLInNewTab:command];

    [IntentDonationHelper donateIntent:IntentType::kOpenNewTab];
  }
}

- (void)createNewTabGroupFromView:(UIView*)sender {
  // Create an empty Tab Group.
  [self createNewTabGroupWithTabs:{}];
}

- (void)assistantButtonTappedWithState:(AppBarAssistantButtonState)state {
  switch (state) {
    case AppBarAssistantButtonState::kLens: {
      OpenLensInputSelectionCommand* command =
          [[OpenLensInputSelectionCommand alloc]
                  initWithEntryPoint:LensEntrypoint::AppBar
                   presentationStyle:LensInputSelectionPresentationStyle::
                                         SlideFromRight
              presentationCompletion:nil];
      [self.lensHandler openLensInputSelection:command];
      break;
    }
    case AppBarAssistantButtonState::kAsk: {
      if (!_authenticationService->HasPrimaryIdentity()) {
        ShowSigninCommand* command = [[ShowSigninCommand alloc]
            initWithOperation:AuthenticationOperation::kSigninOnly
                  accessPoint:signin_metrics::AccessPoint::kIosAppBar];
        [self.sceneHandler showSignin:command
                   baseViewController:self.baseViewController];
        return;
      }
      if (!_geminiService || (!_geminiService->IsProfileEligibleForGemini() &&
                              _geminiService->GeminiIneligibilityForProfile()
                                  .value()
                                  .account_capability)) {
        // TODO(crbug.com/484000888): If user is not eligible, then show prompt
        // notifying ineligibility.
        return;
      }
      GeminiStartupState* startupState = [[GeminiStartupState alloc]
          initWithEntryPoint:gemini::EntryPoint::AppBar];
      [self.geminiHandler startGeminiFlowWithStartupState:startupState];
      break;
    }
    case AppBarAssistantButtonState::kAIM: {
      [self.sceneHandler showAssistant];
      break;
    }
  }
}

#pragma mark - ToolbarButtonMenuFactoryDelegate

- (void)addNewTabInCurrentTabGroup {
  if (!self.currentTabGroup) {
    return;
  }

  [self.tabGridHandler prepareToExitTabGrid];
  if ([self addNewTabInGroup:self.currentTabGroup
                   incognito:_incognitoState.incognitoContentVisible]) {
    [self.tabGridHandler exitTabGrid];
  }
}

- (void)navigateToPageForItem:(web::NavigationItem*)item {
  // App bar does not have web navigation functionality in its button menus.
  NOTREACHED();
}

#pragma mark - Properties

- (void)setCurrentWebStateList:(WebStateList*)currentWebStateList {
  if (_currentWebStateList) {
    _currentWebStateList->RemoveObserver(_observerBridge.get());
  }

  if (currentWebStateList != _currentWebStateList) {
    self.currentTabGroup = _tabGridState.visibleTabGroup
                               ? GetGroupForActiveWebState(currentWebStateList)
                               : nullptr;
    _currentWebStateList = currentWebStateList;
  }

  if (_currentWebStateList) {
    _currentWebStateList->AddObserver(_observerBridge.get());
  }
  [self updateConsumer];
}

- (void)setCurrentTabGroup:(const TabGroup*)currentTabGroup {
  if (_currentTabGroup == currentTabGroup) {
    return;
  }
  _currentTabGroup = currentTabGroup;
  [self updateConsumer];
}

#pragma mark - Private

// Updates the consumer with the current state of the web state list.
- (void)updateConsumer {
  if (!self.consumer || !self.currentWebStateList) {
    return;
  }
  NSUInteger tabCount;

  // Determine the tab count to display in the tab grid button.
  if (self.currentTabGroup) {
    tabCount = static_cast<NSUInteger>(self.currentTabGroup->range().count());
  } else {
    tabCount = static_cast<NSUInteger>(self.currentWebStateList->count());
  }

  BOOL incognito = self.currentWebStateList == _incognitoWebStateList;
  ToolbarButtonMenuFactory* buttonMenuFactory =
      incognito ? _incognitoButtonMenuFactory : _regularButtonMenuFactory;

  [self.consumer setMenu:[buttonMenuFactory menuForAssistantButton]
           forButtonType:AppBarButtonTypeAssistant];
  [self.consumer setMenu:[buttonMenuFactory menuForNewTabButton]
           forButtonType:AppBarButtonTypeNewTab];
  [self.consumer setMenu:[buttonMenuFactory menuForTabGridButton]
           forButtonType:AppBarButtonTypeTabGrid];

  [self.consumer updateTabCount:tabCount];
  [self.consumer setTabGridVisible:_tabGridState.tabGridVisible];
  [self.consumer setTabGroupsPageVisible:_tabGridState.currentPage ==
                                         TabGridPageTabGroups];
  [self.consumer setTabGroupVisible:self.currentTabGroup != nullptr];
  [self.consumer
      setInTabGroup:GetGroupForActiveWebState(self.currentWebStateList)];

  [self updateAssistantButton];
  [self updateButtonsForCurrentTabGridPage];
}

// Updates for entering tab grid `page`.
- (void)updateForTabGridPage:(TabGridPage)page {
  _currentPage = page;
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.currentWebStateList = _incognitoWebStateList;
      break;
    case TabGridPageRegularTabs:
      self.currentWebStateList = _regularWebStateList;
      break;
    case TabGridPageTabGroups:
      CHECK_NE(TabGridPageTabGroups, _tabGridState.originPage);
      [self updateForTabGridPage:_tabGridState.originPage];
      break;
  }
  [self updateButtonsForCurrentTabGridPage];
}

// Updates the buttons in the tab grid.
- (void)updateButtonsForCurrentTabGridPage {
  TabGridPage page = _currentPage == TabGridPageTabGroups
                         ? _tabGridState.originPage
                         : _currentPage;
  BOOL isIncognitoPage = page == TabGridPageIncognitoTabs;
  BOOL enableButtons =
      IsAddNewTabAllowedByPolicy(_prefService, isIncognitoPage);
  BOOL isIncognitoContentVisible =
      (!_tabGridState.tabGridVisible &&
       _incognitoState.incognitoContentVisible) ||
      (_tabGridState.tabGridVisible && isIncognitoPage);
  if (isIncognitoContentVisible) {
    enableButtons = enableButtons && !_incognitoState.authenticationRequired;
  }
  [self.consumer setButtonsEnabled:enableButtons];
  [self.consumer setIncognito:isIncognitoContentVisible];
}

// Updates the consumer with the latest state of the assistant button.
- (void)updateAssistantButton {
  AppBarAssistantButtonState state = AppBarAssistantButtonState::kLens;

  if (IsPageActionMenuEnabled()) {
    state = AppBarAssistantButtonState::kAsk;
  } else if (IsAimCobrowseEnabled() && IsAssistantContainerEnabled()) {
    state = AppBarAssistantButtonState::kAIM;
  }

  [self.consumer setAssistantButtonState:state];
}

// Updates for `incognito` being visible.
- (void)updateForIncognitoVisible:(BOOL)incognitoVisible {
  _currentPage =
      incognitoVisible ? TabGridPageIncognitoTabs : TabGridPageRegularTabs;
  if (incognitoVisible) {
    self.currentWebStateList = _incognitoWebStateList;
  } else {
    self.currentWebStateList = _regularWebStateList;
  }
}

// Opens a new tab in the TabGrid in `incognito`.
- (void)openNewTabInTabGridInIncognito:(BOOL)incognito {
  CHECK(_tabGridState.tabGridVisible);

  // Ignore the tap if the current page is disabled for some reason, by policy
  // for instance. This is to avoid situations where the tap action from an
  // enabled page can make it to a disabled page by releasing the
  // button press after switching to the disabled page (b/273416844 is an
  // example).
  if (!IsAddNewTabAllowedByPolicy(_prefService, incognito)) {
    return;
  }

  if (_tabGridState.visibleTabGroup) {
    id<TabGroupsCommands> tabGroupsHandler =
        incognito ? self.incognitoTabGroupsCommands
                  : self.regularTabGroupsCommands;
    [tabGroupsHandler hideTabGroup];
  }

  [self.tabGridHandler prepareToExitTabGrid];
  base::RecordAction(base::UserMetricsAction("MobileTabNewTab"));
  // Shows the tab only if has been created.
  if ([self addNewTabIncognito:incognito]) {
    [self.tabGridHandler exitTabGrid];
    if (incognito) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridCreateIncognitoTab"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridCreateRegularTab"));
    }
  } else {
    if (incognito) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridFailedCreateIncognitoTab"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridFailedCreateRegularTab"));
    }
  }
}

// Adds a new tab in `incognito` and returns its success.
- (BOOL)addNewTabIncognito:(BOOL)incognito {
  WebStateList* webStateList =
      incognito ? _incognitoWebStateList : _regularWebStateList;
  int webStateListCount = webStateList->count();

  // There are some circumstances where a new tab insertion can be erroniously
  // triggered while another web state list mutation is happening. To ensure
  // those bugs don't become crashes, check that the web state list is OK to
  // mutate.
  if (webStateList->IsMutating()) {
    // Shouldn't have happened!
    NOTREACHED(base::NotFatalUntil::M154) << "Reentrant web state insertion!";
    return false;
  }

  CHECK(_URLLoader);

  UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
  params.in_incognito = incognito;
  params.append_to = OpenPosition::kLastTab;
  params.switch_mode_if_needed = true;
  _URLLoader->Load(params);

  return webStateListCount != webStateList->count();
}

// Triggers the creation of a New Tab Group with 'identifiers'.
- (void)createNewTabGroupWithTabs:(std::set<web::WebStateID>)identifiers {
  // While in the tab grid, the App Bar can only create new Tab Groups from the
  // Tab Groups page in the grid.
  CHECK(!_tabGridState.tabGridVisible ||
        _tabGridState.currentPage == TabGridPageTabGroups);
  CHECK(self.regularTabGroupsCommands);
  CHECK(self.incognitoTabGroupsCommands);

  // If the current tab is incognito and visible, then a new Tab Group being
  // created should be incognito. If the current tab is incognito, but not
  // visible (the Tab Grid is visible), then a Tab Group is being created from
  // the Tab Groups page so the new group should be non-incognito.
  BOOL isCurrentTabIncognitoAndVisible =
      self.currentWebStateList == _incognitoWebStateList &&
      !_tabGridState.tabGridVisible;

  id<TabGroupsCommands> tabGroupsHandler = isCurrentTabIncognitoAndVisible
                                               ? self.incognitoTabGroupsCommands
                                               : self.regularTabGroupsCommands;

  // Create a new Tab Group.
  base::RecordAction(base::UserMetricsAction("MobileTabGridCreateTabGroup"));
  if (identifiers.empty()) {
    // Create an empty Tab Group.
    [tabGroupsHandler showTabGroupCreationWithoutTabs];
  } else {
    // Create a Tab Group with 'identifiers'.
    [tabGroupsHandler showTabGroupCreationForTabs:identifiers];
  }
}

// Adds a new tab in `group` and returns its success.
- (BOOL)addNewTabInGroup:(const TabGroup*)group incognito:(BOOL)incognito {
  CHECK(group);
  WebStateList* webStateList =
      incognito ? _incognitoWebStateList : _regularWebStateList;
  int webStateListCount = webStateList->count();

  GURL URL(kChromeUINewTabURL);
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.in_incognito = incognito;
  params.load_in_group = true;
  params.tab_group = group->GetWeakPtr();
  _URLLoader->Load(params);

  [self updateConsumer];
  return webStateListCount != webStateList->count();
}

@end
