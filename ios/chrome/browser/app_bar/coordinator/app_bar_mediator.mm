// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
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
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/web_state.h"
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
  raw_ptr<PrefService> _prefService;
  raw_ptr<UrlLoadingBrowserAgent> _URLLoader;
  TabGridPage _currentPage;
  TabGridState* _tabGridState;
  IncognitoState* _incognitoState;
}

- (instancetype)initWithRegularWebStateList:(WebStateList*)regularWebStateList
                      incognitoWebStateList:(WebStateList*)incognitoWebStateList
                                prefService:(PrefService*)prefService
                                  URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                               tabGridState:(TabGridState*)tabGridState
                             incognitoState:(IncognitoState*)incognitoState {
  self = [super init];
  if (self) {
    _regularWebStateList = regularWebStateList;
    _incognitoWebStateList = incognitoWebStateList;
    _observerBridge = std::make_unique<WebStateListObserverBridge>(self);

    _URLLoader = URLLoader;
    _prefService = prefService;

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

- (void)setConsumer:(id<AppBarConsumer>)consumer {
  _consumer = consumer;
  [self updateConsumer];
}

- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList {
  _incognitoWebStateList = incognitoWebStateList;
  if (_tabGridState.tabGridVisible &&
      _currentPage == TabGridPageIncognitoTabs) {
    self.currentWebStateList = _incognitoWebStateList;
    [self updateConsumer];
  } else if (_incognitoState.incognitoContentVisible) {
    self.currentWebStateList = _incognitoWebStateList;
  }
}

- (void)disconnect {
  self.consumer = nil;
  if (self.currentWebStateList) {
    self.currentWebStateList->RemoveObserver(_observerBridge.get());
    self.currentWebStateList = nullptr;
  }
  _observerBridge.reset();
  _regularWebStateList = nullptr;
  _incognitoWebStateList = nullptr;
  _prefService = nullptr;
  _URLLoader = nullptr;
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

#pragma mark - TabGridStateObserver

- (void)willEnterTabGrid {
  _currentPage = _tabGridState.currentPage;
  self.currentTabGroup = _tabGridState.visibleTabGroup;
  [self.consumer setTabGridVisible:YES];
}

- (void)willExitTabGrid {
  [self.consumer setTabGridVisible:NO];
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
}

// Updates for entering tab grid `page`.
- (void)updateForTabGridPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      [self.consumer setTabGroupsPageVisible:NO];
      self.currentWebStateList = _incognitoWebStateList;
      break;
    case TabGridPageRegularTabs:
      [self.consumer setTabGroupsPageVisible:NO];
      self.currentWebStateList = _regularWebStateList;
      break;
    case TabGridPageTabGroups:
      CHECK_NE(TabGridPageTabGroups, _tabGridState.originPage);
      CHECK_EQ(TabGridPageTabGroups, _currentPage);
      [self updateForTabGridPage:_tabGridState.originPage];
      [self.consumer setTabGroupsPageVisible:YES];
      break;
  }
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

@end
