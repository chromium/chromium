// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>
#import <optional>
#import <set>

#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/search/search.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/avatar/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface AppBarMediator () <AuthenticationServiceObserving,
                              IdentityManagerObserverBridgeDelegate,
                              IncognitoStateObserver,
                              TabGridStateObserver,
                              WebStateListObserving>

// The web state list currently observed by this mediator.
@property(nonatomic, assign) WebStateList* currentWebStateList;

// The TabGroup currently visible.
@property(nonatomic, assign) const TabGroup* currentTabGroup;

@end

@implementation AppBarMediator {
  std::unique_ptr<WebStateListObserverBridge> _observerBridge;
  std::unique_ptr<AuthenticationServiceObserverBridge> _authServiceBridge;
  std::unique_ptr<signin::IdentityManagerObserverBridge> _identityManagerBridge;
  raw_ptr<WebStateList> _regularWebStateList;
  raw_ptr<WebStateList> _incognitoWebStateList;
  raw_ptr<FullscreenController> _regularFullscreenController;
  std::unique_ptr<FullscreenUIUpdater> _regularFullscreenUIUpdater;
  raw_ptr<FullscreenController> _incognitoFullscreenController;
  std::unique_ptr<FullscreenUIUpdater> _incognitoFullscreenUIUpdater;
  raw_ptr<PrefService> _prefService;
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<BwgService> _geminiService;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<UrlLoadingBrowserAgent> _URLLoader;
  raw_ptr<TemplateURLService> _templateURLService;
  TabGridPage _currentPage;
  TabGridState* _tabGridState;
  IncognitoState* _incognitoState;
}

- (instancetype)
      initWithRegularWebStateList:(WebStateList*)regularWebStateList
            incognitoWebStateList:(WebStateList*)incognitoWebStateList
      regularFullscreenController:
          (FullscreenController*)regularFullscreenController
    incognitoFullscreenController:
        (FullscreenController*)incognitoFullscreenController
                      prefService:(PrefService*)prefService
               templateURLService:(TemplateURLService*)templateURLService
            authenticationService:(AuthenticationService*)authenticationService
                    geminiService:(BwgService*)geminiService
            accountManagerService:
                (ChromeAccountManagerService*)accountManagerService
                  identityManager:(signin::IdentityManager*)identityManager
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

    _URLLoader = URLLoader;
    _prefService = prefService;
    _templateURLService = templateURLService;

    _authenticationService = authenticationService;
    _authServiceBridge = std::make_unique<AuthenticationServiceObserverBridge>(
        _authenticationService, self);

    _geminiService = geminiService;
    _accountManagerService = accountManagerService;

    _identityManager = identityManager;
    _identityManagerBridge =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);

    _tabGridState = tabGridState;
    [_tabGridState addObserver:self];

    _incognitoState = incognitoState;
    [_incognitoState addObserver:self];

    if (_tabGridState.tabGridVisible) {
      [self updateForTabGridPage:_tabGridState.currentPage];
    } else {
      [self updateForIncognitoVisible:_incognitoState.incognitoContentVisible];
    }
  }
  return self;
}

- (void)setConsumer:(id<AppBarConsumer, FullscreenUIElement>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _regularFullscreenUIUpdater.reset();
  _incognitoFullscreenUIUpdater.reset();
  _consumer = consumer;
  if (!_consumer) {
    return;
  }
  _regularFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      _regularFullscreenController, _consumer);
  _incognitoFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      _incognitoFullscreenController, _consumer);
  [self updateConsumer];
}

- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList {
  _incognitoWebStateList = incognitoWebStateList;
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

- (void)disconnect {
  self.consumer = nil;
  self.currentTabGroup = nullptr;
  if (self.currentWebStateList) {
    self.currentWebStateList->RemoveObserver(_observerBridge.get());
    self.currentWebStateList = nullptr;
  }
  _regularFullscreenUIUpdater.reset();
  _incognitoFullscreenUIUpdater.reset();
  _regularFullscreenController = nullptr;
  _incognitoFullscreenController = nullptr;
  [_tabGridState removeObserver:self];
  [_incognitoState removeObserver:self];
  _identityManager = nullptr;
  _authServiceBridge.reset();
  _identityManagerBridge.reset();
  _observerBridge.reset();
  _regularWebStateList = nullptr;
  _incognitoWebStateList = nullptr;
  _prefService = nullptr;
  _templateURLService = nullptr;
  _authenticationService = nullptr;
  _geminiService = nullptr;
  _accountManagerService = nullptr;
  _URLLoader = nullptr;
  _incognitoState = nil;
  _tabGridState = nil;
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
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
  [self updateConsumer];
}

- (void)willExitTabGrid {
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
  self.currentTabGroup = nullptr;
  if (!_tabGridState.tabGridVisible) {
    return;
  }
  [self updateForTabGridPage:_tabGridState.currentPage];
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  [self updateAssistantButton];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  [self updateAssistantButton];
}

- (void)onAccountsOnDeviceChanged {
  [self updateAssistantButton];
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  [self updateAssistantButton];
}

- (void)onIdentityManagerShutdown:(signin::IdentityManager*)identityManager {
  _identityManager = nullptr;
  _identityManagerBridge.reset();
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
    case AppBarAssistantButtonState::kSignedOut: {
      ShowSigninCommand* command = [[ShowSigninCommand alloc]
          initWithOperation:AuthenticationOperation::kSigninOnly
                accessPoint:signin_metrics::AccessPoint::kIosAppBar];
      [self.sceneHandler showSignin:command
                 baseViewController:self.baseViewController];
      break;
    }
    case AppBarAssistantButtonState::kAccount: {
      [self.settingsHandler
          showAccountsSettingsFromViewController:self.baseViewController
                            skipIfUINotAvailable:NO];
      break;
    }
    case AppBarAssistantButtonState::kAsk: {
      if (!_authenticationService->HasPrimaryIdentity(
              signin::ConsentLevel::kSignin)) {
        // TODO(crbug.com/484000888): Prompt user to sign in.
        return;
      }
      if (!_geminiService || !_geminiService->IsProfileEligibleForGemini()) {
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
      [self.sceneHandler
          showAssistantWithContext:[CobrowseContext defaultContext]];
      break;
    }
  }
}

#pragma mark - Properties

- (void)setCurrentWebStateList:(WebStateList*)currentWebStateList {
  if (_currentWebStateList) {
    _currentWebStateList->RemoveObserver(_observerBridge.get());
  }
  _currentWebStateList = currentWebStateList;
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
  if (self.currentTabGroup) {
    tabCount = static_cast<NSUInteger>(self.currentTabGroup->range().count());
  } else {
    tabCount = self.currentWebStateList->count();
  }
  [self.consumer updateTabCount:tabCount];
  [self.consumer setTabGridVisible:_tabGridState.tabGridVisible];
  [self.consumer setTabGroupsPageVisible:_currentPage == TabGridPageTabGroups];
  [self.consumer setTabGroupVisible:_tabGridState.visibleTabGroup];
  [self.consumer setInTabGroup:[self activeWebStateInGroup]];

  [self.consumer setMenu:[self createContextMenuForAssistantButton]
           forButtonType:AppBarButtonTypeAssistant];
  [self.consumer setMenu:[self createContextMenuForNewTabButton]
           forButtonType:AppBarButtonTypeNewTab];
  [self.consumer setMenu:[self createContextMenuForTabGridButton]
           forButtonType:AppBarButtonTypeTabGrid];
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
    if (IsIOSSoftLockEnabled()) {
      // TODO(crbug.com/484000564): Hide background if authentication is
      // required.
    }
  }
  [self.consumer setButtonsEnabled:enableButtons];
}

// Updates the consumer with the latest state of the assistant button.
- (void)updateAssistantButton {
  AppBarAssistantButtonState state = AppBarAssistantButtonState::kSignedOut;
  UIImage* avatar = nil;

  if (IsPageActionMenuEnabled()) {
    state = AppBarAssistantButtonState::kAsk;
  } else if (IsAimCobrowseEnabled() && IsAssistantContainerEnabled()) {
    state = AppBarAssistantButtonState::kAIM;
  } else if (_authenticationService->HasPrimaryIdentity(
                 signin::ConsentLevel::kSignin)) {
    state = AppBarAssistantButtonState::kAccount;
    id<SystemIdentity> identity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    ApplicationContext* context = GetApplicationContext();
    signin::AvatarProvider* avatarProvider =
        context ? context->GetIdentityAvatarProvider() : nullptr;
    if (avatarProvider) {
      avatar = avatarProvider->GetIdentityAvatar(
          identity, IdentityAvatarSize::TableViewIcon);
    }
  }

  [self.consumer setAssistantButtonState:state avatar:avatar];
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
  params.switch_mode_if_needed = false;
  _URLLoader->Load(params);

  return webStateListCount != webStateList->count();
}

// Adds a new tab to the current tab group.
- (void)addNewTabInCurrentTabGroup {
  if (!self.currentTabGroup) {
    return;
  }

  GURL URL(kChromeUINewTabURL);
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.in_incognito = _incognitoState.incognitoContentVisible;
  params.load_in_group = true;
  params.tab_group = self.currentTabGroup->GetWeakPtr();
  _URLLoader->Load(params);
  [self updateConsumer];
}

// Returns whether the active web state in the current web state list is in a
// tab group.
- (BOOL)activeWebStateInGroup {
  if (!self.currentWebStateList) {
    return NO;
  }
  int activeIndex = self.currentWebStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex) {
    return NO;
  }
  return self.currentWebStateList->GetGroupOfWebStateAt(activeIndex) != nullptr;
}

// Returns the context menu for the Assistant button.
// TODO(crbug.com/484000556) Implement this menu.
- (UIMenu*)createContextMenuForAssistantButton {
  return nil;
}

// Returns the context menu for the New Tab button.
- (UIMenu*)createContextMenuForNewTabButton {
  CHECK(self.regularActionFactory);
  CHECK(self.incognitoActionFactory);

  BOOL isTabGroupsPageVisible = _currentPage == TabGridPageTabGroups;
  BOOL isTabGroupVisible = _tabGridState.visibleTabGroup;

  BrowserActionFactory* actionFactory = _incognitoState.incognitoContentVisible
                                            ? self.incognitoActionFactory
                                            : self.regularActionFactory;

  __weak __typeof(self) weakSelf = self;
  ProceduralBlock createNewTabBlock = ^{
    [weakSelf createNewTabFromView:nil];
  };
  UIAction* newTabAction =
      _incognitoState.incognitoContentVisible
          ? [actionFactory
                actionToOpenNewIncognitoTabWithBlock:createNewTabBlock]
          : [actionFactory actionToOpenNewTabWithBlock:createNewTabBlock];
  newTabAction.image =
      DefaultSymbolWithPointSize(kPlusSymbol, kSymbolActionPointSize);

  // Context menu for when a tab group is open in the tab grid.
  if (isTabGroupVisible) {
    UIAction* newTabInCurrentGroupAction =
        [actionFactory actionToAddNewTabInGroupWithBlock:^{
          [weakSelf addNewTabInCurrentTabGroup];
        }];

    return
        [UIMenu menuWithChildren:@[ newTabAction, newTabInCurrentGroupAction ]];
  }

  // Context menu for when the tab groups page is visible in the tab grid.
  if (isTabGroupsPageVisible) {
    UIAction* newTabGroupAction =
        [actionFactory actionToCreateEmptyTabGroupWithBlock:^{
          [weakSelf createNewTabGroupFromView:nil];
        }];
    newTabGroupAction.title =
        l10n_util::GetNSString(IDS_IOS_APP_BAR_CONTEXT_MENU_NEW_TAB_GROUP);

    return [UIMenu menuWithChildren:@[ newTabGroupAction, newTabAction ]];
  }

  // The New Tab button should not have a context menu while viewing the regular
  // or incognito tab pages (unless looking inside a tab group).
  if (_tabGridState.tabGridVisible) {
    return nil;
  }

  // Context menu for while browsing.
  CHECK(_templateURLService);
  const bool useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::PlusButton,
          search::DefaultSearchProviderIsGoogle(_templateURLService));

  UIAction* newSearchAction = [actionFactory actionToStartNewSearch];
  UIAction* newIncognitoSearchAction =
      [actionFactory actionToStartNewIncognitoSearch];
  UIAction* voiceSearchAction = [actionFactory actionToStartVoiceSearch];
  UIAction* cameraSearchAction =
      useLens
          ? [actionFactory
                actionToSearchWithLensWithEntryPoint:LensEntrypoint::PlusButton]
          : [actionFactory actionToShowQRScanner];

  NSMutableArray* staticActions = [NSMutableArray arrayWithArray:@[
    newSearchAction, newIncognitoSearchAction, voiceSearchAction,
    cameraSearchAction
  ]];

  if (IsAIMCobrowseDebugEntrypointEnabled()) {
    UIAction* openAIMode = [actionFactory actionToOpenAIMode];
    [staticActions addObject:openAIMode];
  }

  if (experimental_flags::EnableAIPrototypingMenu()) {
    UIAction* openAIMenu = [actionFactory actionToOpenAIMenu];
    [staticActions addObject:openAIMenu];
  }

  UIMenuElement* clipboardAction = [self createMenuElementForPasteboard];
  if (clipboardAction) {
    UIMenu* staticMenu = [UIMenu menuWithTitle:@""
                                         image:nil
                                    identifier:nil
                                       options:UIMenuOptionsDisplayInline
                                      children:staticActions];
    return [UIMenu menuWithChildren:@[ staticMenu, clipboardAction ]];
  }

  return [UIMenu menuWithTitle:@""
                         image:nil
                    identifier:nil
                       options:UIMenuOptionsDisplayInline
                      children:staticActions];
}

// Returns the context menu for the Tab Grid button.
- (UIMenu*)createContextMenuForTabGridButton {
  CHECK(self.regularActionFactory);
  CHECK(self.incognitoActionFactory);

  // If the tab grid is showing, the context menu should be disabled.
  if (_tabGridState.tabGridVisible) {
    return nil;
  }

  NSMutableArray* staticActions = [[NSMutableArray alloc] init];

  UIAction* closeCurrentTabAction =
      _incognitoState.incognitoContentVisible
          ? [self.incognitoActionFactory actionToCloseCurrentTab]
          : [self.regularActionFactory actionToCloseCurrentTab];
  [staticActions addObject:closeCurrentTabAction];

  if (base::FeatureList::IsEnabled(kTabGroupInTabIconContextMenu)) {
    UIMenuElement* tabGroupActionsMenu = [self createMenuElementForTabGroups];
    if (tabGroupActionsMenu) {
      [staticActions addObject:tabGroupActionsMenu];
    }
  }

  // From an incognito tab, the `openNewTabAction` should open a non-incognito
  // tab. From a non-incognito tab, it should open an incognito tab.
  UIAction* openNewTabAction =
      _incognitoState.incognitoContentVisible
          ? [self.regularActionFactory actionToOpenNewTab]
          : [self.incognitoActionFactory actionToOpenNewIncognitoTab];
  [staticActions addObject:openNewTabAction];

  return [UIMenu menuWithTitle:@""
                         image:nil
                    identifier:nil
                       options:UIMenuOptionsDisplayInline
                      children:staticActions];
}

// Returns the UIMenuElement for the content of the pasteboard. Can return
// `nil`.
- (UIMenuElement*)createMenuElementForPasteboard {
  CHECK(self.regularActionFactory);
  CHECK(self.incognitoActionFactory);

  BrowserActionFactory* actionFactory = _incognitoState.incognitoContentVisible
                                            ? self.incognitoActionFactory
                                            : self.regularActionFactory;

  std::optional<std::set<ClipboardContentType>> clipboardContentType =
      ClipboardRecentContent::GetInstance()->GetCachedClipboardContentTypes();

  if (clipboardContentType.has_value()) {
    std::set<ClipboardContentType> clipboardContentTypeValues =
        clipboardContentType.value();

    if (clipboardContentTypeValues.contains(ClipboardContentType::Image)) {
      if (base::FeatureList::IsEnabled(kEnableLensInOmniboxCopiedImage)) {
        if (search_engines::SupportsSearchImageWithLens(_templateURLService) &&
            ios::provider::IsLensSupported()) {
          return [actionFactory actionToLensCopiedImage];
        }
      } else {
        if (search_engines::SupportsSearchByImage(_templateURLService)) {
          return [actionFactory actionToSearchCopiedImage];
        }
      }
    } else if (clipboardContentTypeValues.contains(ClipboardContentType::URL)) {
      return [actionFactory actionToSearchCopiedURL];
    } else if (clipboardContentTypeValues.contains(
                   ClipboardContentType::Text)) {
      return [actionFactory actionToSearchCopiedText];
    }
  }
  return nil;
}

// Returns the UIMenuElement for the content of the Add/Move Tab to Group Menu.
- (UIMenuElement*)createMenuElementForTabGroups {
  CHECK(base::FeatureList::IsEnabled(kTabGroupInTabIconContextMenu));
  CHECK(self.currentWebStateList);
  CHECK(self.regularActionFactory);
  CHECK(self.incognitoActionFactory);

  BrowserActionFactory* actionFactory = _incognitoState.incognitoContentVisible
                                            ? self.incognitoActionFactory
                                            : self.regularActionFactory;

  int activeIndex = self.currentWebStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex) {
    return nil;
  }

  std::set<const TabGroup*> allGroups = self.currentWebStateList->GetGroups();
  const TabGroup* currentGroup =
      self.currentWebStateList->GetGroupOfWebStateAt(activeIndex);

  __weak __typeof(self) weakSelf = self;
  /// If the current tab is in a group, display the "Move Tab to Group" menu.
  /// Otherwise, display the "Add Tab to Group" menu. If a user doesn't have
  /// any Tab Groups, the "Add Tab to Group" menu will just be an "Add Tab to
  /// New Group" button.
  if (currentGroup) {
    return [actionFactory menuToMoveTabToGroupWithGroups:allGroups
        currentGroup:currentGroup
        moveBlock:^(const TabGroup* destinationGroup) {
          [weakSelf moveTabToGroupBlock:destinationGroup];
        }
        removeBlock:^{
          [weakSelf removeTabFromGroupBlock];
        }];
  } else {
    return [actionFactory
        menuToAddTabToGroupWithGroups:allGroups
                         numberOfTabs:1
                                block:^(const TabGroup* destinationGroup) {
                                  [weakSelf
                                      addTabToGroupBlock:destinationGroup];
                                }];
  }
}

// Creates a Move Tab to Group block for the Move Tab to Group menu.
- (void)moveTabToGroupBlock:(const TabGroup*)destinationGroup {
  CHECK(base::FeatureList::IsEnabled(kTabGroupInTabIconContextMenu));
  CHECK([self activeWebStateInGroup]);
  int tabIndex = self.currentWebStateList->active_index();
  self.currentWebStateList->MoveToGroup({tabIndex}, destinationGroup);
}

// Creates a Remove Tab from Group block for the Move Tab to Group menu.
- (void)removeTabFromGroupBlock {
  CHECK(base::FeatureList::IsEnabled(kTabGroupInTabIconContextMenu));
  CHECK([self activeWebStateInGroup]);
  int tabIndex = self.currentWebStateList->active_index();
  self.currentWebStateList->RemoveFromGroups({tabIndex});
}

// Creates an Add Tab to Group block for the Add Tab to Group menu.
- (void)addTabToGroupBlock:(const TabGroup*)destinationGroup {
  CHECK(base::FeatureList::IsEnabled(kTabGroupInTabIconContextMenu));
  CHECK(![self activeWebStateInGroup]);
  int tabIndex = self.currentWebStateList->active_index();
  if (destinationGroup) {
    self.currentWebStateList->MoveToGroup({tabIndex}, destinationGroup);
  } else {
    web::WebState* currentWebState =
        self.currentWebStateList->GetActiveWebState();
    if (!currentWebState) {
      return;
    }
    std::set<web::WebStateID> identifiers = {
        currentWebState->GetUniqueIdentifier()};
    [self createNewTabGroupWithTabs:identifiers];
  }
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

@end
