// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/search/search.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface AppBarMediator () <IncognitoStateObserver,
                              TabGridStateObserver,
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
  raw_ptr<PrefService> _prefService;
  raw_ptr<UrlLoadingBrowserAgent> _URLLoader;
  raw_ptr<TemplateURLService> _templateURLService;
  TabGridPage _currentPage;
  TabGridState* _tabGridState;
  IncognitoState* _incognitoState;
}

- (instancetype)initWithRegularWebStateList:(WebStateList*)regularWebStateList
                      incognitoWebStateList:(WebStateList*)incognitoWebStateList
                regularFullscreenController:
                    (FullscreenController*)regularFullscreenController
              incognitoFullscreenController:
                  (FullscreenController*)incognitoFullscreenController
                                prefService:(PrefService*)prefService
                         templateURLService:
                             (TemplateURLService*)templateURLService
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
  _observerBridge.reset();
  _regularWebStateList = nullptr;
  _incognitoWebStateList = nullptr;
  _prefService = nullptr;
  _templateURLService = nullptr;
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
  self.currentWebStateList = _incognitoWebStateList;
}

- (void)willExitIncognitoForState:(IncognitoState*)incognitoState {
  if (_tabGridState.tabGridVisible) {
    return;
  }
  self.currentWebStateList = _regularWebStateList;
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
  if (!_tabGridState.tabGridVisible) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileTabGridCreateTabGroup"));
  [self.regularTabGroupsCommands showTabGroupCreationWithoutTabs];
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
  [self updateButtonsForCurrentTabGridPage];
}

// Updates for entering tab grid `page`.
- (void)updateForTabGridPage:(TabGridPage)page {
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
  BOOL enableButtons = IsAddNewTabAllowedByPolicy(
      _prefService, page == TabGridPageIncognitoTabs);
  if (page == TabGridPageIncognitoTabs) {
    enableButtons = enableButtons && !_incognitoState.authenticationRequired;
    if (IsIOSSoftLockEnabled()) {
      // TODO(crbug.com/484000564): Hide background if authentication is
      // required.
    }
  }
  [self.consumer setButtonsEnabled:enableButtons];
}

// Updates for `incognito` being visible.
- (void)updateForIncognitoVisible:(BOOL)incognitoVisible {
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

  // TODO(crbug.com/484000878): Add experimental menu buttons.
  return [UIMenu menuWithChildren:@[
    newSearchAction, newIncognitoSearchAction, voiceSearchAction,
    cameraSearchAction
  ]];
}

// Returns the context menu for the Tab Grid button.
- (UIMenu*)createContextMenuForTabGridButton {
  CHECK(self.regularActionFactory);
  CHECK(self.incognitoActionFactory);

  // If the tab grid is showing, the context menu should be disabled.
  if (_tabGridState.tabGridVisible) {
    return nil;
  }

  // From an incognito tab, the `openNewTabAction` should open a non-incognito
  // tab. From a non-incognito tab, it should open an incognito tab.
  UIAction* openNewTabAction =
      _incognitoState.incognitoContentVisible
          ? [self.regularActionFactory actionToOpenNewTab]
          : [self.incognitoActionFactory actionToOpenNewIncognitoTab];
  UIAction* closeCurrentTabAction =
      _incognitoState.incognitoContentVisible
          ? [self.incognitoActionFactory actionToCloseCurrentTab]
          : [self.regularActionFactory actionToCloseCurrentTab];

  return [UIMenu menuWithChildren:@[ closeCurrentTabAction, openNewTabAction ]];
}

@end
