// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/create_tab_group_mediator.h"

#import <memory>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_multi_source_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/saved_tab_groups/ui/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/create_tab_group_mediator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_creation_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon_configurator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/web_state_tab_switcher_item.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

@interface CreateTabGroupMediator () <WebStateListObserving>
@end

@implementation CreateTabGroupMediator {
  // Consumer of the tab group creator;
  __weak id<TabGroupCreationConsumer> _consumer;
  // List of tabs to add to the tab group.
  std::set<web::WebStateID> _identifiers;
  // Web state list where the tab group belong.
  raw_ptr<WebStateList> _webStateList;
  // Tab group to edit.
  raw_ptr<const TabGroup, DanglingUntriaged> _tabGroup;
  // Item to fetch pictures.
  TabGroupItem* _groupItem;
  // Helper class to configure tab item images.
  std::unique_ptr<TabSnapshotAndFaviconConfigurator> _tabImagesConfigurator;
  // Source browser. Only set when creating a new group, not when editing an
  // existing one.
  raw_ptr<Browser> _browser;
  // Observers for WebStateList. Only set when editing an existing group,
  // when creating a new one.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<
      base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>
      _scopedWebStateListObservation;
  // Whether a new tab should be inserted into the new group.
  BOOL _createNewTabForGroup;
}

- (instancetype)
    initTabGroupCreationWithConsumer:(id<TabGroupCreationConsumer>)consumer
                        selectedTabs:(std::set<web::WebStateID>&)identifiers
                             browser:(Browser*)browser
                       faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    CHECK(consumer);
    if (!_createNewTabForGroup) {
      CHECK(!identifiers.empty()) << "Cannot create an empty tab group.";
    }
    CHECK(browser);
    _identifiers = identifiers;

    _browser = browser;
    _webStateList = browser->GetWebStateList();
    _consumer = consumer;
    [_consumer setDefaultGroupColor:TabGroup::DefaultColorForNewTabGroup(
                                        _webStateList)];
    [_consumer setTabsCount:_identifiers.size()];
    _tabImagesConfigurator =
        std::make_unique<TabSnapshotAndFaviconConfigurator>(
            faviconLoader, SnapshotBrowserAgent::FromBrowser(browser));
    ProfileIOS* profile = browser->GetProfile();
    BrowserList* browserList = BrowserListFactory::GetForProfile(profile);

    NSInteger numberOfRequestedImages = 0;
    for (web::WebStateID identifier : identifiers) {
      if (numberOfRequestedImages >= 7) {
        break;
      }
      WebStateList* currentWebStateList = _webStateList;
      // TODO(crbug.com/333032676): Replace this with the appropriate helper
      // once it exists.
      int index = GetWebStateIndex(
          _webStateList, WebStateSearchCriteria{.identifier = identifier});
      if (index == WebStateList::kInvalidIndex) {
        // The user is creating a group from a long press on search result. Tab
        // search can display all tabs from the same profile at the same time.
        // The selected tab is currently in a different web state list (inactive
        // tab, or tab from another window).
        Browser* selectedTabBrowser = GetBrowserForTabWithCriteria(
            browserList, WebStateSearchCriteria{.identifier = identifier},
            profile->IsOffTheRecord());
        CHECK(selectedTabBrowser);
        currentWebStateList = selectedTabBrowser->GetWebStateList();
        index =
            GetWebStateIndex(currentWebStateList,
                             WebStateSearchCriteria{.identifier = identifier});
      }

      NSInteger tabIndexRequested = numberOfRequestedImages;
      __weak CreateTabGroupMediator* weakSelf = self;
      _tabImagesConfigurator->FetchSingleSnapshotAndFaviconFromWebState(
          currentWebStateList->GetWebStateAt(index),
          ^(TabSnapshotAndFavicon* tabSnapshotAndFavicon) {
            [weakSelf configureTabSnapshotAndFavicon:tabSnapshotAndFavicon
                                            tabIndex:tabIndexRequested];
          });
      numberOfRequestedImages++;
    }
  }
  return self;
}

- (instancetype)
    initEmptyTabGroupCreationWithConsumer:(id<TabGroupCreationConsumer>)consumer
                                  browser:(Browser*)browser
                            faviconLoader:(FaviconLoader*)faviconLoader {
  _createNewTabForGroup = YES;
  std::set<web::WebStateID> identifiers;
  return [self initTabGroupCreationWithConsumer:consumer
                                   selectedTabs:identifiers
                                        browser:browser
                                  faviconLoader:faviconLoader];
}

- (instancetype)initTabGroupEditionWithConsumer:
                    (id<TabGroupCreationConsumer>)consumer
                                       tabGroup:(const TabGroup*)tabGroup
                                        browser:(Browser*)browser
                                  faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    CHECK(consumer);
    CHECK(tabGroup);
    CHECK(browser);
    _consumer = consumer;
    _tabGroup = tabGroup;
    _webStateList = browser->GetWebStateList();
    // Observe the WebStateList in the case the group disappears.
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObservation = std::make_unique<
        base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>(
        _webStateListObserverBridge.get());
    _scopedWebStateListObservation->AddObservation(_webStateList);
    _groupItem = [[TabGroupItem alloc] initWithTabGroup:_tabGroup];
    _tabImagesConfigurator =
        std::make_unique<TabSnapshotAndFaviconConfigurator>(
            faviconLoader, SnapshotBrowserAgent::FromBrowser(browser));

    __weak CreateTabGroupMediator* weakSelf = self;

    // Do not use the helper to get the following values as the title helper do
    // not return nil but the number of tabs. In this case, we want nil so it do
    // not display anything.
    tab_groups::TabGroupVisualData visualData = _tabGroup->visual_data();
    [_consumer setDefaultGroupColor:visualData.color()];
    [_consumer setGroupTitle:base::SysUTF16ToNSString(visualData.title())];
    [_consumer setTabsCount:_tabGroup->range().count()];

    _tabImagesConfigurator->FetchSnapshotAndFaviconForTabGroupItem(
        _groupItem, _webStateList,
        ^(TabGroupItem* item, NSInteger tabIndex,
          TabSnapshotAndFavicon* tabSnapshotAndFavicon) {
          [weakSelf configureTabSnapshotAndFavicon:tabSnapshotAndFavicon
                                          tabIndex:tabIndex];
        });
  }
  return self;
}

- (void)disconnect {
  if (_tabGroup) {
    _scopedWebStateListObservation->RemoveAllObservations();
    _scopedWebStateListObservation.reset();
    _webStateListObserverBridge.reset();
    _webStateList = nullptr;
  }
}

#pragma mark - TabGroupCreationMutator

// TODO(crbug.com/40942154): Rename the function to better match what it does.
- (void)createNewGroupWithTitle:(NSString*)title
                          color:(tab_groups::TabGroupColorId)colorID
                     completion:(void (^)())completion {
  tab_groups::TabGroupVisualData visualData =
      tab_groups::TabGroupVisualData(base::SysNSStringToUTF16(title), colorID);
  if (_tabGroup) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGroupUserUpdatedGroup"));
    if (![_tabGroup->GetRawTitle() isEqualToString:title]) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGroupUserUpdatedGroupName"));
    }
    if (_tabGroup->GetColor() != colorID) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGroupUserUpdatedGroupColor"));
    }
    _webStateList->UpdateGroupVisualData(_tabGroup, visualData);
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGroupUserCreatedNewGroup"));
    WebStateList::ScopedBatchOperation lock =
        _webStateList->StartBatchOperation();
    if (_createNewTabForGroup) {
      CHECK(_identifiers.empty());
      // Insert a new tab before creating the group to prevent empty groups.
      id<ApplicationCommands> dispatcher = HandlerForProtocol(
          _browser->GetCommandDispatcher(), ApplicationCommands);
      OpenNewTabCommand* command = [OpenNewTabCommand command];
      [dispatcher openURLInNewTab:command];

      web::WebState* activeWebState = _webStateList->GetActiveWebState();
      CHECK(activeWebState);
      _identifiers.insert(activeWebState->GetUniqueIdentifier());
      CHECK(!_identifiers.empty());
    }
    std::set<int> tabIndexes;
    for (web::WebStateID identifier : _identifiers) {
      int index = GetWebStateIndex(_webStateList, WebStateSearchCriteria{
                                                      .identifier = identifier,
                                                  });
      if (index == WebStateList::kInvalidIndex) {
        index = _webStateList->count();
        MoveTabToBrowser(identifier, _browser, index);
      }
      tabIndexes.insert(index);
    }
    if (!tabIndexes.empty()) {
      _webStateList->CreateGroup(tabIndexes, visualData,
                                 tab_groups::TabGroupId::GenerateNew());
    }
  }
  completion();
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  CHECK_EQ(_webStateList, webStateList);
  switch (change.type()) {
    case WebStateListChange::Type::kGroupVisualDataUpdate: {
      const WebStateListChangeGroupVisualDataUpdate& visualDataUpdate =
          change.As<WebStateListChangeGroupVisualDataUpdate>();
      if (_tabGroup == visualDataUpdate.updated_group()) {
        // Dismiss the editor.
        [self.delegate
            createTabGroupMediatorEditedGroupWasExternallyMutated:self];
      }
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& deletion =
          change.As<WebStateListChangeGroupDelete>();
      if (_tabGroup == deletion.deleted_group()) {
        // Dismiss the editor.
        [self.delegate
            createTabGroupMediatorEditedGroupWasExternallyMutated:self];
      }
      break;
    }
    default:
      // No-op.
      break;
  }
}

#pragma mark - Private

// Configures the `tabSnapshotAndFavicon` for the tab at `tabIndex`.
- (void)configureTabSnapshotAndFavicon:
            (TabSnapshotAndFavicon*)tabSnapshotAndFavicon
                              tabIndex:(NSInteger)tabIndex {
  [_consumer setSnapshotAndFavicon:tabSnapshotAndFavicon tabIndex:tabIndex];
}

@end
