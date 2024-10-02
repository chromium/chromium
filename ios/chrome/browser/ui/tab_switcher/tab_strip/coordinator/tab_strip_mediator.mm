// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_mediator.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_last_tab_dragged_alert_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_action_type.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_mediator_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_features_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/chrome/browser/web_state_list/model/web_state_list_favicon_driver_observer.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "ui/gfx/image/image.h"

namespace {

// Finds any TabGroup in `web_state_list` whose range starts at `index`.
// Returns `nullptr` if no such TabGroup exists.
const TabGroup* FindTabGroupStartingAtIndex(int index,
                                            WebStateList* web_state_list) {
  CHECK(web_state_list);
  for (const TabGroup* group : web_state_list->GetGroups()) {
    if (group->range().range_begin() == index) {
      return group;
    }
  }
  return nullptr;
}

// Returns the `TabStripItemData` for a tab item at `index` in `web_state_list`.
TabStripItemData* CreateTabItemData(int index, WebStateList* web_state_list) {
  CHECK(web_state_list);
  CHECK(web_state_list->ContainsIndex(index), base::NotFatalUntil::M128);
  const TabGroup* group = web_state_list->GetGroupOfWebStateAt(index);
  TabStripItemData* data = [[TabStripItemData alloc] init];
  if (group) {
    const TabGroupRange range = group->range();
    data.isFirstTabInGroup = range.range_begin() == index;
    data.isLastTabInGroup = range.range_end() == index + 1;
    data.groupStrokeColor = group->GetColor();
  }
  return data;
}

// Returns the `TabStripItemData` for `group`.
TabStripItemData* CreateGroupItemData(const TabGroup* group) {
  TabStripItemData* data = [[TabStripItemData alloc] init];
  data.groupStrokeColor = group->GetColor();
  return data;
}

// Returns the `TabStripItemData` elements for WebStates and TabGroups in
// `range` in `web_state_list`. If `including_groups` is set to false, then
// TabGroups are not included in the result.
NSMutableArray<TabStripItemData*>* CreateItemData(
    WebStateList* web_state_list,
    bool including_hidden_tab_items = true,
    bool including_group_items = true,
    TabGroupRange range = TabGroupRange::InvalidRange()) {
  CHECK(web_state_list);
  if (!range.valid()) {
    range = {0, web_state_list->count()};
  }
  CHECK_GE(range.range_begin(), 0);
  CHECK_LE(range.range_end(), web_state_list->count());
  NSMutableArray<TabStripItemData*>* data = [[NSMutableArray alloc] init];
  for (int index : range) {
    const TabGroup* group_of_web_state = nullptr;
    if ([TabStripFeaturesUtils isModernTabStripWithTabGroups]) {
      CHECK(web_state_list->ContainsIndex(index), base::NotFatalUntil::M128);
      group_of_web_state = web_state_list->GetGroupOfWebStateAt(index);
      if (including_group_items) {
        const TabGroup* group_starting_at_index =
            FindTabGroupStartingAtIndex(index, web_state_list);
        if (group_starting_at_index) {
          [data addObject:CreateGroupItemData(group_starting_at_index)];
        }
      }
    }
    // The tab associated with WebState at `index` should be included in the
    // output if it has no group, or its group is not collapsed, or
    // `including_hidden_tab_items` is true.
    const bool should_include_tab_item =
        !group_of_web_state ||
        !group_of_web_state->visual_data().is_collapsed() ||
        including_hidden_tab_items;
    if (should_include_tab_item) {
      [data addObject:CreateTabItemData(index, web_state_list)];
    }
  }
  return data;
}

// Returns the `TabStripItemIdentifier` elements for WebStates and TabGroups in
// `range` in `web_state_list`. If `including_groups` is set to false, then
// TabGroups are not included in the result.
NSMutableArray<TabStripItemIdentifier*>* CreateItemIdentifiers(
    WebStateList* web_state_list,
    bool including_hidden_tab_items = true,
    bool including_group_items = true,
    TabGroupRange range = TabGroupRange::InvalidRange()) {
  CHECK(web_state_list);
  if (!range.valid()) {
    range = {0, web_state_list->count()};
  }
  CHECK_GE(range.range_begin(), 0);
  CHECK_LE(range.range_end(), web_state_list->count());
  NSMutableArray<TabStripItemIdentifier*>* item_identifiers =
      [[NSMutableArray alloc] init];
  for (int index : range) {
    const TabGroup* group_of_web_state = nullptr;
    if ([TabStripFeaturesUtils isModernTabStripWithTabGroups]) {
      CHECK(web_state_list->ContainsIndex(index), base::NotFatalUntil::M128);
      group_of_web_state = web_state_list->GetGroupOfWebStateAt(index);
      if (including_group_items) {
        const TabGroup* group_starting_at_index =
            FindTabGroupStartingAtIndex(index, web_state_list);
        if (group_starting_at_index) {
          [item_identifiers
              addObject:CreateGroupItemIdentifier(group_starting_at_index,
                                                  web_state_list)];
        }
      }
    }
    // The tab associated with WebState at `index` should be included in the
    // output if it has no group, or its group is not collapsed, or
    // `including_hidden_tab_items` is true.
    const bool should_include_tab_item =
        !group_of_web_state ||
        !group_of_web_state->visual_data().is_collapsed() ||
        including_hidden_tab_items;
    if (should_include_tab_item) {
      web::WebState* web_state = web_state_list->GetWebStateAt(index);
      [item_identifiers addObject:CreateTabItemIdentifier(web_state)];
    }
  }
  return item_identifiers;
}

}  // namespace

@interface TabStripMediator () <CRWWebStateObserver,
                                WebStateFaviconDriverObserver,
                                WebStateListObserving>
// The consumer for this object.
@property(nonatomic, weak) id<TabStripConsumer> consumer;

@end

@implementation TabStripMediator {
  // Bridge C++ WebStateListObserver methods to this TabStripController.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // Bridge C++ WebStateObserver methods to this TabStripController.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Forward observer methods for all WebStates in the WebStateList monitored
  // by the TabStripMediator.
  std::unique_ptr<AllWebStateObservationForwarder>
      _allWebStateObservationForwarder;
  // Bridges FaviconDriverObservers methods to this mediator, and maintains a
  // FaviconObserver for each all webstates.
  std::unique_ptr<WebStateListFaviconDriverObserver>
      _webStateListFaviconObserver;
  // Browser list.
  raw_ptr<BrowserList> _browserList;

  // List of items in the tab strip when a drag operation starts.
  // Should be set back to `nil` when the drag operation ends.
  NSMutableArray<TabStripItemIdentifier*>* _dragItems;

  // Used to get info about saved groups and to mutate them.
  raw_ptr<tab_groups::TabGroupSyncService> _tabGroupSyncService;
}

- (instancetype)initWithConsumer:(id<TabStripConsumer>)consumer
             tabGroupSyncService:
                 (tab_groups::TabGroupSyncService*)tabGroupSyncService
                     browserList:(BrowserList*)browserList {
  if ((self = [super init])) {
    CHECK(browserList);
    _browserList = browserList;
    _tabGroupSyncService = tabGroupSyncService;
    _consumer = consumer;
  }
  return self;
}

#pragma mark - Public methods

- (void)disconnect {
  if (_webStateList) {
    [self removeWebStateObservations];
    _webStateListFaviconObserver.reset();
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
  }
  _tabStripHandler = nil;
  _browserList = nullptr;
}

- (void)cancelMoveForTab:(web::WebStateID)tabID
           originBrowser:(Browser*)originBrowser
             originIndex:(int)originIndex
              visualData:(const tab_groups::TabGroupVisualData&)visualData
            localGroupID:(const tab_groups::TabGroupId&)localGroupID
                 savedID:(const base::Uuid&)savedID {
  BrowserAndIndex browserAndIndex = FindBrowserAndIndex(
      tabID, _browserList->BrowsersOfType(BrowserList::BrowserType::kRegular));
  if (!browserAndIndex.browser || !originBrowser) {
    return;
  }

  originIndex =
      std::min(originIndex, originBrowser->GetWebStateList()->count() - 1);
  if (!originBrowser->GetWebStateList()->ContainsIndex(originIndex)) {
    return;
  }

  if (!_tabGroupSyncService) {
    return;
  }
  std::optional<tab_groups::SavedTabGroup> savedGroup =
      _tabGroupSyncService->GetGroup(savedID);
  if (!savedGroup || savedGroup->local_group_id() ||
      savedGroup->saved_tabs().size() != 1) {
    // Don't cancel if the saved group has been modified (deleted, associated
    // with another local group or changed its tabs).
    return;
  }

  const WebStateList::InsertionParams insertionParams =
      WebStateList::InsertionParams::AtIndex(originIndex);
  MoveTabToBrowser(tabID, originBrowser, insertionParams);

  // Move to the new group
  BrowserAndIndex browserAndIndexAfterMove = FindBrowserAndIndex(
      tabID, _browserList->BrowsersOfType(BrowserList::BrowserType::kRegular));
  if (!browserAndIndexAfterMove.browser) {
    return;
  }
  WebStateList* afterMoveWebStateList =
      browserAndIndexAfterMove.browser->GetWebStateList();
  const int afterMoveIndex = browserAndIndexAfterMove.tab_index;

  const web::WebState* const webState =
      afterMoveWebStateList->GetWebStateAt(afterMoveIndex);
  const std::u16string title = webState->GetTitle();
  const GURL url = webState->GetVisibleURL();

  {
    // As this is a "undo" the usual mechanisms should be paused as the tab
    // moved back to its original position shouldn't be treated as a "new" tab
    // added to the group.
    auto localObservationPauser =
        _tabGroupSyncService->CreateScopedLocalObserverPauser();

    afterMoveWebStateList->CreateGroup({afterMoveIndex}, visualData,
                                       localGroupID);
    _tabGroupSyncService->UpdateLocalTabGroupMapping(
        savedID, localGroupID, tab_groups::OpeningSource::kCancelCloseLastTab);

    // In case the tab has changed (URL or title), update it.
    _tabGroupSyncService->UpdateLocalTabId(
        localGroupID, savedGroup->saved_tabs()[0].saved_tab_guid(),
        tabID.identifier());

    tab_groups::SavedTabGroupTabBuilder tab_builder;
    tab_builder.SetURL(url);
    tab_builder.SetTitle(title);
    _tabGroupSyncService->UpdateTab(localGroupID, tabID.identifier(),
                                    std::move(tab_builder));
  }
}

- (void)deleteSavedGroupWithID:(const base::Uuid&)savedID {
  _tabGroupSyncService->RemoveGroup(savedID);
}

- (void)ungroupGroup:(TabGroupItem*)tabGroupItem {
  if (!self.webStateList || !tabGroupItem.tabGroup) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileTabStripUngroupTabs"));
  self.webStateList->DeleteGroup(tabGroupItem.tabGroup);
}

- (void)deleteGroup:(TabGroupItem*)tabGroupItem {
  if (!self.webStateList || !tabGroupItem.tabGroup) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileTabStripDeleteGroup"));
  CloseAllWebStatesInGroup(*_webStateList, tabGroupItem.tabGroup,
                           WebStateList::CLOSE_USER_ACTION);
}

#pragma mark - Public properties

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    [self removeWebStateObservations];
    _webStateListFaviconObserver.reset();
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;

  if (_webStateList) {
    DCHECK_GE(_webStateList->count(), 0);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    _webStateListFaviconObserver =
        std::make_unique<WebStateListFaviconDriverObserver>(_webStateList,
                                                            self);

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    [self addWebStateObservations];
  }

  [self populateConsumerItems];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  bool activeWebStateDidChangeStatus = false;
  bool activeWebStateDidMove = false;

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly: {
      // The activation is handled after this switch statement.
      if (!status.active_web_state_change()) {
        const WebStateListChangeStatusOnly& statusOnlyChange =
            change.As<WebStateListChangeStatusOnly>();
        [self moveItemForWebState:statusOnlyChange.web_state()
            fromWebStateListIndex:statusOnlyChange.index()
              toWebStateListIndex:statusOnlyChange.index()
                         oldGroup:statusOnlyChange.old_group()
                         newGroup:statusOnlyChange.new_group()];
        if (statusOnlyChange.index() == webStateList->active_index()) {
          activeWebStateDidChangeStatus = true;
        }
      }
      break;
    }
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detachChange =
          change.As<WebStateListChangeDetach>();
      web::WebState* detachedWebState = detachChange.detached_web_state();
      TabStripItemIdentifier* item = [TabStripItemIdentifier
          tabIdentifier:[[WebStateTabSwitcherItem alloc]
                            initWithWebState:detachedWebState]];
      [self.consumer removeItems:@[ item ]];
      // Reconfigure the group items if needed.
      const TabGroup* group = detachChange.group();
      if (group) {
        [self updateDataAndReconfigureItemsInGroup:group];
      }
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insertChange =
          change.As<WebStateListChangeInsert>();
      const int index = insertChange.index();
      web::WebState* insertedWebState = insertChange.inserted_web_state();
      TabStripItemIdentifier* itemIdentifier =
          CreateTabItemIdentifier(insertedWebState);
      TabStripItemData* itemData = CreateTabItemData(index, webStateList);
      [self.consumer updateItemData:@{itemIdentifier : itemData}
                   reconfigureItems:NO];
      const TabGroup* group = insertChange.group();

      if (TabStripItemIdentifier* previousItemIdentifier =
              [self destinationItemAtIndex:(index - 1) parentGroup:group]) {
        // If after the update, there is a neighbor WebState to the left of the
        // newly inserted WebState, then there are three cases where
        // `previousItemIdentifier` is not nil.
        // 1. `group` is not nil and the neighbor is in `group` too
        // (`previousItemIdentifier` is the neighbor WebState).
        // 2. `group` is nil and the neighbor is not in a group either
        // (`previousItemIdentifier` is the neighbor WebState).
        // 3. `group` is nil and the neighbor is in a different group
        // (`previousItemIdentifier` is the neighbor WebState's group).
        [self.consumer insertItems:@[ itemIdentifier ]
                         afterItem:previousItemIdentifier];
      } else if (TabStripItemIdentifier* nextItemIdentifier =
                     [self destinationItemAtIndex:(index + 1)
                                      parentGroup:group]) {
        // If after the update, there is a neighbor WebState to the right of the
        // newly inserted WebState, then there are the same three cases where
        // `nextItemIdentifier` is not nil.
        [self.consumer insertItems:@[ itemIdentifier ]
                        beforeItem:nextItemIdentifier];
      } else if (group) {
        // If there is no neighbor WebState in `group` to insert before/after,
        // then the item will be the first child of that group.
        TabGroupItem* groupItem =
            [[TabGroupItem alloc] initWithTabGroup:group
                                      webStateList:webStateList];
        [self.consumer insertItems:@[ itemIdentifier ] insideGroup:groupItem];
      } else if (const TabGroup* emptyGroupAtIndexZero =
                     FindTabGroupStartingAtIndex(0, _webStateList)) {
        // If `group` is null but there is no neighbor WebState to insert
        // before/after, then the WebStateList has no WebStates and this new
        // WebState is inserted at index 0. If there is an empty group at index
        // 0, the item should be inserted after that group.
        TabStripItemIdentifier* groupItemIdentifier =
            CreateGroupItemIdentifier(emptyGroupAtIndexZero, _webStateList);
        [self.consumer insertItems:@[ itemIdentifier ]
                         afterItem:groupItemIdentifier];
      } else {
        // If `group` is null, there are no WebStates in the WebStateList and
        // there is no empty group at index 0, then the new item should be
        // inserted at the beginning of the collection view.
        [self.consumer insertItems:@[ itemIdentifier ] afterItem:nil];
      }

      // Reconfigure the group items if needed.
      if (group) {
        [self updateDataAndReconfigureItemsInGroup:group];
      }
      break;
    }
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& moveChange =
          change.As<WebStateListChangeMove>();
      [self moveItemForWebState:moveChange.moved_web_state()
          fromWebStateListIndex:moveChange.moved_from_index()
            toWebStateListIndex:moveChange.moved_to_index()
                       oldGroup:moveChange.old_group()
                       newGroup:moveChange.new_group()];
      if (moveChange.moved_to_index() == webStateList->active_index()) {
        activeWebStateDidMove = true;
      }
      break;
    }
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replaceChange =
          change.As<WebStateListChangeReplace>();
      const int index = replaceChange.index();
      TabSwitcherItem* oldItem = [[WebStateTabSwitcherItem alloc]
          initWithWebState:replaceChange.replaced_web_state()];
      web::WebState* newWebState = replaceChange.inserted_web_state();
      TabSwitcherItem* newItem =
          [[WebStateTabSwitcherItem alloc] initWithWebState:newWebState];
      TabStripItemIdentifier* newItemIdentifier =
          [TabStripItemIdentifier tabIdentifier:newItem];
      TabStripItemData* newItemData = CreateTabItemData(index, webStateList);
      [self.consumer updateItemData:@{newItemIdentifier : newItemData}
                   reconfigureItems:NO];
      [self.consumer replaceItem:oldItem withItem:newItem];
      break;
    }
    case WebStateListChange::Type::kGroupCreate: {
      const WebStateListChangeGroupCreate& groupCreateChange =
          change.As<WebStateListChangeGroupCreate>();
      const TabGroup* group = groupCreateChange.created_group();
      TabStripItemIdentifier* groupItemIdentifier =
          CreateGroupItemIdentifier(group, webStateList);
      TabStripItemData* groupItemData = CreateGroupItemData(group);
      [self.consumer updateItemData:@{groupItemIdentifier : groupItemData}
                   reconfigureItems:NO];
      // Determine the destination item for the new group item.
      const int pivotIndex = group->range().range_begin();
      TabStripItemIdentifier* destinationItemIdentifier =
          [self destinationItemAtIndex:pivotIndex parentGroup:nullptr];
      [self.consumer insertItems:@[ groupItemIdentifier ]
                      beforeItem:destinationItemIdentifier];
      // Ensure the group item is expanded if the group is not collapsed.
      // This is needed because items in a collection view are collapsed by
      // default.
      if (!group->visual_data().is_collapsed()) {
        [self.consumer expandGroup:groupItemIdentifier.tabGroupItem];
      }
      break;
    }
    case WebStateListChange::Type::kGroupVisualDataUpdate: {
      const WebStateListChangeGroupVisualDataUpdate& visualDataChange =
          change.As<WebStateListChangeGroupVisualDataUpdate>();
      const TabGroup* updatedGroup = visualDataChange.updated_group();
      TabGroupItem* updatedGroupItem =
          [[TabGroupItem alloc] initWithTabGroup:updatedGroup
                                    webStateList:webStateList];
      const bool oldCollapsed =
          visualDataChange.old_visual_data().is_collapsed();
      const bool newCollapsed = updatedGroup->visual_data().is_collapsed();
      if (oldCollapsed != newCollapsed) {
        if (newCollapsed) {
          const bool updateActiveIndex =
              updatedGroup->range().contains(webStateList->active_index());
          if (updateActiveIndex) {
            // If the active WebState will be collapsed, set the `selectItem`
            // to `nil` to ensure a smooth animation.
            [self.consumer selectItem:nil];
          }
          [self.consumer collapseGroup:updatedGroupItem];

          if (updateActiveIndex) {
            // If the active WebState is now collapsed, activate an existing or
            // new non-collapsed WebState.
            __weak __typeof(self) weakSelf = self;
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(^{
                  [weakSelf activateExistingOrNewNonCollapsedWebState];
                }));
          }
        } else {
          [self.consumer expandGroup:updatedGroupItem];
        }
      }
      [self updateDataAndReconfigureItemsInGroup:updatedGroup];
      break;
    }
    case WebStateListChange::Type::kGroupMove: {
      const WebStateListChangeGroupMove& groupMoveChange =
          change.As<WebStateListChangeGroupMove>();
      TabStripItemIdentifier* itemIdentifier = CreateGroupItemIdentifier(
          groupMoveChange.moved_group(), _webStateList);
      const TabGroupRange toRange = groupMoveChange.moved_to_range();
      // Move item to new position.
      if (TabStripItemIdentifier* previousItemIdentifier =
              [self destinationItemAtIndex:(toRange.range_begin() - 1)
                               parentGroup:nil]) {
        // If after the update, there is a neighbor WebState to the left of the
        // group's range, then there are two cases where
        // `previousItemIdentifier` is not nil.
        // 1. The neighbor is not in a group (`previousItemIdentifier` is the
        // neighbor WebState).
        // 2. The neighbor is in a group (`previousItemIdentifier` is the
        // neighbor WebState's group).
        [self.consumer moveItem:itemIdentifier
                      afterItem:previousItemIdentifier];
      } else if (TabStripItemIdentifier* nextItemIdentifier =
                     [self destinationItemAtIndex:toRange.range_end()
                                      parentGroup:nil]) {
        // If after the update, there is a neighbor WebState to the right of the
        // group's range, then there are two cases where
        // `nextItemIdentifier` is not nil.
        // 1. The neighbor is not in a group (`nextItemIdentifier` is the
        // neighbor WebState).
        // 2. The neighbor is in a group (`nextItemIdentifier` is the neighbor
        // WebState's group).
        [self.consumer moveItem:itemIdentifier beforeItem:nextItemIdentifier];
      }
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& groupDeleteChange =
          change.As<WebStateListChangeGroupDelete>();
      const TabGroup* group = groupDeleteChange.deleted_group();
      TabStripItemIdentifier* groupItemIdentifier =
          CreateGroupItemIdentifier(group, webStateList);
      [self.consumer removeItems:@[ groupItemIdentifier ]];
      break;
    }
  }

  // If there is a new active WebState, or the current active WebState moved or
  // changed status, ensure it is still selected and visible i.e. ensure that if
  // it is in a group, then this group is not collapsed.
  if (status.active_web_state_change() || activeWebStateDidMove ||
      activeWebStateDidChangeStatus) {
    const int activeIndex = webStateList->active_index();
    // If the selected index changes as a result of the last webstate being
    // detached, the active index will be -1.
    if (activeIndex == WebStateList::kInvalidIndex) {
      [self.consumer selectItem:nil];
      return;
    }
    TabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
        initWithWebState:status.new_active_web_state];
    [self.consumer selectItem:item];
    // If the active WebState is in a group, ensure that group is not collapsed.
    const TabGroup* groupOfActiveWebState =
        webStateList->GetGroupOfWebStateAt(activeIndex);
    if (groupOfActiveWebState &&
        groupOfActiveWebState->visual_data().is_collapsed()) {
      const tab_groups::TabGroupVisualData oldVisualData =
          groupOfActiveWebState->visual_data();
      const tab_groups::TabGroupVisualData newVisualData{
          oldVisualData.title(), oldVisualData.color(), /*is_collapsed=*/false};
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&WebStateList::UpdateGroupVisualData,
                                    webStateList->AsWeakPtr(),
                                    groupOfActiveWebState, newVisualData));
    }
  }
}

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  [self removeWebStateObservations];
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  [self addWebStateObservations];
  [self populateConsumerItems];
}

#pragma mark - TabStripMutator

- (void)addNewItem {
  if (!self.webStateList || !self.profile) {
    return;
  }
  const auto insertionParams = WebStateList::InsertionParams::Automatic();
  [self insertAndActivateNewWebStateWithInsertionParams:insertionParams];
}

- (void)activateItem:(TabSwitcherItem*)item {
  if (!self.webStateList) {
    return;
  }
  int index =
      GetWebStateIndex(self.webStateList, WebStateSearchCriteria{
                                              .identifier = item.identifier,
                                          });

  _webStateList->ActivateWebStateAt(index);
}

- (void)collapseGroup:(TabGroupItem*)tabGroupItem {
  if (!self.webStateList) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileTabStripGroupCollapse"));
  CHECK(tabGroupItem.tabGroup);
  const tab_groups::TabGroupVisualData oldVisualData =
      tabGroupItem.tabGroup->visual_data();
  const tab_groups::TabGroupVisualData newVisualData{
      oldVisualData.title(), oldVisualData.color(), /*is_collapsed=*/true};
  self.webStateList->UpdateGroupVisualData(tabGroupItem.tabGroup,
                                           newVisualData);
}

- (void)expandGroup:(TabGroupItem*)tabGroupItem {
  if (!self.webStateList) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileTabStripGroupExpand"));
  CHECK(tabGroupItem.tabGroup);
  const tab_groups::TabGroupVisualData oldVisualData =
      tabGroupItem.tabGroup->visual_data();
  const tab_groups::TabGroupVisualData newVisualData{
      oldVisualData.title(), oldVisualData.color(), /*is_collapsed=*/false};
  self.webStateList->UpdateGroupVisualData(tabGroupItem.tabGroup,
                                           newVisualData);
}

- (void)closeItem:(TabSwitcherItem*)item {
  if (!self.webStateList) {
    return;
  }

  int index = GetWebStateIndex(
      self.webStateList,
      WebStateSearchCriteria{
          .identifier = item.identifier,
          .pinned_state = WebStateSearchCriteria::PinnedState::kNonPinned,
      });
  if (index >= 0)
    self.webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

- (void)removeItemFromGroup:(TabSwitcherItem*)item {
  if (!self.webStateList) {
    return;
  }
  int index =
      GetWebStateIndex(self.webStateList, WebStateSearchCriteria{
                                              .identifier = item.identifier,
                                          });
  self.webStateList->RemoveFromGroups({index});
}

- (void)closeAllItemsExcept:(TabSwitcherItem*)item {
  if (!self.webStateList) {
    return;
  }
  int indexToKeep = GetWebStateIndex(self.webStateList,
                                     WebStateSearchCriteria(item.identifier));

  int closedGroupCount = 0;
  if (IsTabGroupSyncEnabled()) {
    for (const TabGroup* group : _webStateList->GetGroups()) {
      // Remove the local tab group mapping if the `indexToKeep` is not in the
      // group.
      if (!group->range().contains(indexToKeep) &&
          _tabGroupSyncService->GetGroup(group->tab_group_id())) {
        _tabGroupSyncService->RemoveLocalTabGroupMapping(
            group->tab_group_id(), tab_groups::ClosingSource::kCloseOtherTabs);
        closedGroupCount++;
      }
    }
  }

  // Closes all non-pinned items except for `item`.
  CloseOtherWebStates(*(self.webStateList), indexToKeep,
                      WebStateList::CLOSE_USER_ACTION);

  // Show the tab group snackbar if some groups have been closed.
  if (IsTabGroupSyncEnabled() && closedGroupCount > 0) {
    [self.tabStripHandler
        showTabStripTabGroupSnackbarAfterClosingGroups:closedGroupCount];
  }
}

- (void)createNewGroupWithItem:(TabSwitcherItem*)item {
  if (!self.webStateList) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("MobileTabStripCreateGroupWithItem"));
  [_tabStripHandler showTabStripGroupCreationForTabs:{item.identifier}];
}

- (void)addItem:(TabSwitcherItem*)item
        toGroup:(const TabGroup*)destinationGroup {
  if (!self.webStateList || !self.profile) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileTabStripAddItemToGroup"));

  const bool incognito = self.profile->IsOffTheRecord();
  Browser* browserOfGroup =
      GetBrowserForGroup(_browserList, destinationGroup, incognito);

  if (self.browser == browserOfGroup) {
    int indexOfWebState =
        GetWebStateIndex(self.webStateList,
                         WebStateSearchCriteria{.identifier = item.identifier});
    self.webStateList->MoveToGroup({indexOfWebState}, destinationGroup);
    return;
  }

  MoveTabToBrowser(
      item.identifier, browserOfGroup,
      WebStateList::InsertionParams::Automatic().InGroup(destinationGroup));
}

- (void)renameGroup:(TabGroupItem*)tabGroupItem {
  if (!self.webStateList) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileTabStripRenameGroup"));
  [_tabStripHandler
      showTabStripGroupEditionForGroup:tabGroupItem.tabGroup->GetWeakPtr()];
}

- (void)addNewTabInGroup:(TabGroupItem*)tabGroupItem {
  if (!self.webStateList || !self.profile) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileTabStripNewTabInGroup"));
  const auto insertionParams =
      WebStateList::InsertionParams::Automatic().InGroup(tabGroupItem.tabGroup);
  [self insertAndActivateNewWebStateWithInsertionParams:insertionParams];
}

- (void)ungroupGroup:(TabGroupItem*)tabGroupItem
          sourceView:(UIView*)sourceView {
  if (IsTabGroupSyncEnabled()) {
    // Show the confirmation dialog only when the tab group sync feature is
    // enabled.
    [_tabStripHandler
        showTabGroupConfirmationForAction:TabGroupActionType::kUngroupTabGroup
                                groupItem:tabGroupItem
                               sourceView:sourceView];
    return;
  }

  [self ungroupGroup:tabGroupItem];
}

- (void)deleteGroup:(TabGroupItem*)tabGroupItem sourceView:(UIView*)sourceView {
  if (IsTabGroupSyncEnabled()) {
    // Show the confirmation dialog only when the tab group sync feature is
    // enabled.
    [_tabStripHandler
        showTabGroupConfirmationForAction:TabGroupActionType::kDeleteTabGroup
                                groupItem:tabGroupItem
                               sourceView:sourceView];
    return;
  }

  [self deleteGroup:tabGroupItem];
}

- (void)closeGroup:(TabGroupItem*)tabGroupItem {
  if (!self.webStateList || !tabGroupItem.tabGroup) {
    return;
  }

  if (IsTabGroupSyncEnabled()) {
    tab_groups::utils::CloseTabGroupLocally(
        tabGroupItem.tabGroup, self.webStateList, _tabGroupSyncService);
    [self.tabStripHandler showTabStripTabGroupSnackbarAfterClosingGroups:1];
  } else {
    CloseAllWebStatesInGroup(*self.webStateList, tabGroupItem.tabGroup,
                             WebStateList::CLOSE_USER_ACTION);
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDidStartLoading:(web::WebState*)webState {
  if (IsVisibleURLNewTabPage(webState)) {
    return;
  }
  [self reconfigureItemForWebState:webState];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self reconfigureItemForWebState:webState];
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [self reconfigureItemForWebState:webState];
}

#pragma mark - WebStateFaviconDriverObserver

- (void)faviconDriver:(favicon::FaviconDriver*)driver
    didUpdateFaviconForWebState:(web::WebState*)webState {
  [self reconfigureItemForWebState:webState];
}

#pragma mark - TabCollectionDragDropHandler

- (UIDragItem*)dragItemForItem:(TabSwitcherItem*)item {
  web::WebState* webState =
      GetWebState(_webStateList, WebStateSearchCriteria{
                                     .identifier = item.identifier,
                                 });
  return CreateTabDragItem(webState);
}

- (UIDragItem*)dragItemForTabGroupItem:(TabGroupItem*)tabGroupItem {
  return CreateTabGroupDragItem(tabGroupItem.tabGroup, self.profile);
}

- (void)dragWillBeginForTabSwitcherItem:(TabSwitcherItem*)item {
  _dragItems = CreateItemIdentifiers(_webStateList,
                                     /*including_hidden_tab_items=*/false);
  // When a tab is dragged, it is visually removed from the collection view.
  [_dragItems removeObject:[TabStripItemIdentifier tabIdentifier:item]];
}

- (void)dragWillBeginForTabGroupItem:(TabGroupItem*)item {
  _dragItems = CreateItemIdentifiers(_webStateList,
                                     /*including_hidden_tab_items=*/false);
  // When a group is dragged, it is visually removed from the collection view,
  // along with all the tabs within that group.
  [_dragItems removeObject:[TabStripItemIdentifier groupIdentifier:item]];
  CHECK(item.tabGroup);
  for (int childWebStateIndex : item.tabGroup->range()) {
    TabStripItemIdentifier* childItemIdentifier = CreateTabItemIdentifier(
        _webStateList->GetWebStateAt(childWebStateIndex));
    [_dragItems removeObject:childItemIdentifier];
  }
}

- (void)dragSessionDidEnd {
  _dragItems = nil;
}

- (UIDropOperation)dropOperationForDropSession:(id<UIDropSession>)session
                                       toIndex:
                                           (NSUInteger)destinationItemIndex {
  UIDragItem* dragItem = session.localDragSession.items.firstObject;

  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);
    if (tabInfo.profile != self.profile) {
      // Tabs from different profiles cannot be dropped.
      return UIDropOperationForbidden;
    }

    if (_profile->IsOffTheRecord() == tabInfo.incognito) {
      return UIDropOperationMove;
    }

    // Tabs of different profiles (regular/incognito) cannot be dropped.
    return UIDropOperationForbidden;
  }

  // Group move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabGroupInfo class]]) {
    TabGroupInfo* tabGroupInfo =
        base::apple::ObjCCast<TabGroupInfo>(dragItem.localObject);
    if (tabGroupInfo.profile != self.profile) {
      // Tabs from different profiles cannot be dropped.
      return UIDropOperationForbidden;
    }
    if (_dragItems && destinationItemIndex < _dragItems.count &&
        _dragItems[destinationItemIndex].tabSwitcherItem) {
      // If the drop originates from the same collection, then it is forbidden
      // to drop a group before an already grouped tab. If the drop originates
      // from a different collection view, a group can be dropped anywhere, but
      // it will be inserted at a valid location.
      int webStateIndex = GetWebStateIndex(
          _webStateList,
          WebStateSearchCriteria{
              .identifier =
                  _dragItems[destinationItemIndex].tabSwitcherItem.identifier});
      if (_webStateList->ContainsIndex(webStateIndex) &&
          _webStateList->GetGroupOfWebStateAt(webStateIndex)) {
        return UIDropOperationForbidden;
      }
    }

    if (self.profile->IsOffTheRecord() == tabGroupInfo.incognito) {
      return UIDropOperationMove;
    }
    // Tabs of different profiles (regular/incognito) cannot be dropped.
    return UIDropOperationForbidden;
  }

  // All URLs originating from Chrome create a new tab (as opposed to moving a
  // tab).
  if ([dragItem.localObject isKindOfClass:[NSURL class]]) {
    return UIDropOperationCopy;
  }

  // URLs are accepted when drags originate from outside Chrome.
  NSArray<NSString*>* acceptableTypes = @[ UTTypeURL.identifier ];
  if ([session hasItemsConformingToTypeIdentifiers:acceptableTypes]) {
    return UIDropOperationCopy;
  }

  // Other UTI types such as image data or file data cannot be dropped.
  return UIDropOperationForbidden;
}

- (void)dropItem:(UIDragItem*)dragItem
               toIndex:(NSUInteger)destinationIndex
    fromSameCollection:(BOOL)fromSameCollection {
  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);
    if (IsTabGroupSyncEnabled()) {
      BrowserAndIndex browserAndIndex = FindBrowserAndIndex(
          tabInfo.tabID,
          _browserList->BrowsersOfType(BrowserList::BrowserType::kRegular));
      if (browserAndIndex.browser) {
        const TabGroup* group =
            browserAndIndex.browser->GetWebStateList()->GetGroupOfWebStateAt(
                browserAndIndex.tab_index);
        if (group && group->range().count() == 1) {
          // `_tabGroupSyncService` is nullptr in incognito.
          const tab_groups::TabGroupId& localID = group->tab_group_id();
          if (_tabGroupSyncService && _tabGroupSyncService->GetGroup(localID)) {
            const base::Uuid savedID =
                _tabGroupSyncService->GetGroup(localID)->saved_guid();

            _tabGroupSyncService->RemoveLocalTabGroupMapping(
                localID, tab_groups::ClosingSource::kCloseLastTab);

            // Trying to move the last tab of group.
            TabStripLastTabDraggedAlertCommand* command =
                [[TabStripLastTabDraggedAlertCommand alloc] init];
            command.tabID = tabInfo.tabID;
            command.originBrowser = browserAndIndex.browser;
            command.originIndex = browserAndIndex.tab_index;
            command.visualData = group->visual_data();
            command.localGroupID = localID;
            command.savedGroupID = savedID;
            [_tabStripHandler showAlertForLastTabDragged:command];
          }
        }
      }
    }
    if (fromSameCollection) {
      base::UmaHistogramEnumeration(kUmaTabStripViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
      // Reorder tabs.
      const WebStateList::InsertionParams insertionParams =
          [self insertionParamsForDestinationItemIndex:destinationIndex
                                                 items:_dragItems];
      MoveWebStateWithIdentifierToInsertionParams(
          tabInfo.tabID, insertionParams, _webStateList, fromSameCollection);
    } else {
      // The tab lives in another Browser.
      // TODO(crbug.com/41488813): Need to be updated for pinned tabs.
      base::UmaHistogramEnumeration(kUmaTabStripViewDragOrigin,
                                    DragItemOrigin::kOtherBrowser);
      [self moveItemWithIDFromDifferentBrowser:tabInfo.tabID
                                       toIndex:destinationIndex];
    }
    return;
  }

  // Group move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabGroupInfo class]]) {
    TabGroupInfo* tabGroupInfo =
        base::apple::ObjCCast<TabGroupInfo>(dragItem.localObject);
    // Early return if the group has been closed during the drag an drop.
    if (!tabGroupInfo.tabGroup) {
      return;
    }
    if (fromSameCollection) {
      base::UmaHistogramEnumeration(kUmaTabStripViewGroupDragOrigin,
                                    DragItemOrigin::kSameCollection);
    } else {
      base::UmaHistogramEnumeration(kUmaTabStripViewGroupDragOrigin,
                                    DragItemOrigin::kOtherBrowser);
    }
    // Determine the tab strip item before which the group should be moved.
    NSArray<TabStripItemIdentifier*>* items = _dragItems;
    if (!items) {
      items = CreateItemIdentifiers(self.webStateList,
                                    /*including_hidden_tab_items=*/false);
    }
    TabStripItemIdentifier* nextItemIdentifier = nil;
    if (destinationIndex < items.count) {
      nextItemIdentifier = items[destinationIndex];
    }
    // Move the group before `nextItemIdentifier`.
    MoveGroupBeforeTabStripItem(tabGroupInfo.tabGroup, nextItemIdentifier,
                                self.browser);
    return;
  }
  base::UmaHistogramEnumeration(kUmaTabStripViewDragOrigin,
                                DragItemOrigin::kOther);

  // Handle URLs from within Chrome synchronously using a local object.
  if ([dragItem.localObject isKindOfClass:[URLInfo class]]) {
    URLInfo* droppedURL = static_cast<URLInfo*>(dragItem.localObject);
    [self insertNewItemAtIndex:destinationIndex withURL:droppedURL.URL];
    return;
  }
}

- (void)dropItemFromProvider:(NSItemProvider*)itemProvider
                     toIndex:(NSUInteger)destinationIndex
          placeholderContext:
              (id<UICollectionViewDropPlaceholderContext>)placeholderContext {
  if (![itemProvider canLoadObjectOfClass:[NSURL class]]) {
    [placeholderContext deletePlaceholder];
    return;
  }
  base::UmaHistogramEnumeration(kUmaTabStripViewDragOrigin,
                                DragItemOrigin::kOther);

  __weak __typeof(self) weakSelf = self;
  auto loadHandler =
      ^(__kindof id<NSItemProviderReading> providedItem, NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
          [placeholderContext deletePlaceholder];
          NSURL* droppedURL = static_cast<NSURL*>(providedItem);
          [weakSelf insertNewItemAtIndex:destinationIndex
                                 withURL:net::GURLWithNSURL(droppedURL)];
        });
      };
  [itemProvider loadObjectOfClass:[NSURL class] completionHandler:loadHandler];
}

#pragma mark - Private

// Adds an observation to every WebState of the current WebSateList.
- (void)addWebStateObservations {
  _allWebStateObservationForwarder =
      std::make_unique<AllWebStateObservationForwarder>(
          _webStateList, _webStateObserver.get());
}

// Removes an observation from every WebState of the current WebSateList.
- (void)removeWebStateObservations {
  _allWebStateObservationForwarder.reset();
}

// Updates the consumer with the list of all items and the selected one.
- (void)populateConsumerItems {
  TabSwitcherItem* selectedItem = nil;
  if (_webStateList->GetActiveWebState()) {
    selectedItem = [[WebStateTabSwitcherItem alloc]
        initWithWebState:_webStateList->GetActiveWebState()];
  }
  NSArray<TabStripItemIdentifier*>* itemIdentifiers =
      CreateItemIdentifiers(_webStateList);
  // Prepare tab strip item data (group stroke color, etc).
  NSDictionary<TabStripItemIdentifier*, TabStripItemData*>* itemData =
      [NSDictionary dictionaryWithObjects:CreateItemData(_webStateList)
                                  forKeys:itemIdentifiers];
  // Prepare item parents.
  NSMutableDictionary<TabStripItemIdentifier*, TabGroupItem*>* itemParents =
      [NSMutableDictionary dictionary];
  for (int index = 0; index < _webStateList->count(); ++index) {
    if (const TabGroup* parentGroup =
            _webStateList->GetGroupOfWebStateAt(index)) {
      TabGroupItem* parentTabGroupItem =
          [[TabGroupItem alloc] initWithTabGroup:parentGroup
                                    webStateList:_webStateList];
      TabStripItemIdentifier* itemIdentifier =
          CreateTabItemIdentifier(_webStateList->GetWebStateAt(index));
      [itemParents setObject:parentTabGroupItem forKey:itemIdentifier];
    }
  }
  [self.consumer populateWithItems:itemIdentifiers
                      selectedItem:selectedItem
                          itemData:itemData
                       itemParents:itemParents];
}

// Moves item to the desired final item index `itemIndexAfterUpdate`.
- (void)moveItemWithIDFromDifferentBrowser:(web::WebStateID)sourceWebStateID
                                   toIndex:(NSUInteger)itemIndexAfterUpdate {
  NSMutableArray<TabStripItemIdentifier*>* items = CreateItemIdentifiers(
      _webStateList, /*including_hidden_tab_items=*/false);

  if (itemIndexAfterUpdate >= items.count) {
    MoveTabToBrowser(sourceWebStateID, self.browser, _webStateList->count());
    return;
  }

  const WebStateList::InsertionParams insertionParams =
      [self insertionParamsForDestinationItemIndex:itemIndexAfterUpdate
                                             items:items];
  MoveTabToBrowser(sourceWebStateID, self.browser, insertionParams);
}

// Inserts a new item with the given`newTabURL` at `index`.
- (void)insertNewItemAtIndex:(NSUInteger)index withURL:(const GURL&)newTabURL {
  // There are some circumstances where a new tab insertion can be erroniously
  // triggered while another web state list mutation is happening. To ensure
  // those bugs don't become crashes, check that the web state list is OK to
  // mutate.
  if (_webStateList->IsMutating()) {
    // Shouldn't have happened!
    DCHECK(false) << "Reentrant web state insertion!";
    return;
  }
  DCHECK(_profile);

  web::WebState::CreateParams params(_profile);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  web::NavigationManager::WebLoadParams loadParams(newTabURL);
  loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  // Simulating the insertion.
  NSMutableArray<TabStripItemIdentifier*>* items = CreateItemIdentifiers(
      _webStateList, /*including_hidden_tab_items=*/false);
  WebStateList::InsertionParams insertionParams =
      [self insertionParamsForDestinationItemIndex:index items:items];
  insertionParams.activate = true;
  _webStateList->InsertWebState(std::move(webState), insertionParams);
}

// Returns `InsertionParams` which can be used to move/insert a WebState so as
// to reflect the move/insertion of a new item at `destinationItemIndex` in
// `items`.
- (WebStateList::InsertionParams)
    insertionParamsForDestinationItemIndex:(NSUInteger)destinationItemIndex
                                     items:(NSArray<TabStripItemIdentifier*>*)
                                               items {
  TabStripItemIdentifier* previousItem =
      destinationItemIndex > 0 ? items[destinationItemIndex - 1] : nil;
  TabStripItemIdentifier* nextItem =
      destinationItemIndex < items.count ? items[destinationItemIndex] : nil;
  const TabGroup* destinationGroup =
      [self groupForInsertionBetweenPreviousItem:previousItem
                                        nextItem:nextItem];
  int webStateListInsertionIndex = 0;
  for (NSUInteger itemIndex = 0; itemIndex < destinationItemIndex;
       itemIndex++) {
    if (items[itemIndex].itemType == TabStripItemTypeTab) {
      webStateListInsertionIndex++;
      continue;
    }
    const TabGroup* group = items[itemIndex].tabGroupItem.tabGroup;
    if (group->visual_data().is_collapsed()) {
      webStateListInsertionIndex += group->range().count();
    }
  }
  return WebStateList::InsertionParams::AtIndex(webStateListInsertionIndex)
      .InGroup(destinationGroup);
}

// Returns the appropriate destination group for an item inserted between
// `previousItem` and `nextItem`.
- (const TabGroup*)
    groupForInsertionBetweenPreviousItem:(TabStripItemIdentifier*)previousItem
                                nextItem:(TabStripItemIdentifier*)nextItem {
  if (!nextItem.tabSwitcherItem) {
    // If the next item is not a tab, then the inserted item should have no
    // group.
    return nullptr;
  }

  const int indexOfNextWebState = GetWebStateIndex(
      _webStateList, WebStateSearchCriteria{
                         .identifier = nextItem.tabSwitcherItem.identifier,
                     });
  const TabGroup* groupOfNextWebState =
      _webStateList->GetGroupOfWebStateAt(indexOfNextWebState);
  if (!groupOfNextWebState) {
    // If the next item is not in a group, then the inserted item should have no
    // group either.
    return nullptr;
  }

  if (previousItem.tabGroupItem) {
    if (previousItem.tabGroupItem.tabGroup == groupOfNextWebState) {
      // If the previous item is the parent group item of the next item, then
      // the inserted item should be in that group.
      return groupOfNextWebState;
    }
    return nullptr;
  }

  // If the previous item is not a group item then it is a tab.
  int indexOfPreviousWebState = GetWebStateIndex(
      _webStateList, WebStateSearchCriteria{
                         .identifier = previousItem.tabSwitcherItem.identifier,
                     });
  if (!_webStateList->ContainsIndex(indexOfPreviousWebState)) {
    return nullptr;
  }
  const TabGroup* groupOfPreviousWebState =
      _webStateList->GetGroupOfWebStateAt(indexOfPreviousWebState);
  if (groupOfPreviousWebState == groupOfNextWebState) {
    // If the item is inserted between two tabs belonging to the same group,
    // then it should be inserted in that group.
    return groupOfNextWebState;
  }

  return nullptr;
}

// For each WebState in `group`, the associated item is reconfigured with an
// up-to-date `TabStripItemData`.
- (void)updateDataAndReconfigureItemsInGroup:(const TabGroup*)group {
  const TabGroupRange range = group->range();
  NSArray<TabStripItemIdentifier*>* tabItemIdentifiers =
      CreateItemIdentifiers(_webStateList, /*including_hidden_tab_items=*/false,
                            /*including_group_items=*/true, range);
  NSArray<TabStripItemData*>* tabItemData =
      CreateItemData(_webStateList, /*including_hidden_tab_items=*/false,
                     /*including_group_items=*/true, range);
  NSDictionary<TabStripItemIdentifier*, TabStripItemData*>* tabItemDataDict =
      [NSDictionary dictionaryWithObjects:tabItemData
                                  forKeys:tabItemIdentifiers];
  [self.consumer updateItemData:tabItemDataDict reconfigureItems:YES];
}

// Reconfigures the item associated with `webState`.
- (void)reconfigureItemForWebState:(web::WebState*)webState {
  const int webStateIndex = _webStateList->GetIndexOfWebState(webState);
  if (!_webStateList->ContainsIndex(webStateIndex)) {
    return;
  }
  const TabGroup* group = _webStateList->GetGroupOfWebStateAt(webStateIndex);
  if (group && group->visual_data().is_collapsed()) {
    // If group is collapsed then tab cells cannot be reconfigured.
    return;
  }
  [self.consumer reconfigureItems:@[ CreateTabItemIdentifier(webState) ]];
}

// Returns a destination item for insertion before/after `index`, such that the
// inserted item becomes a child of `parentGroup`. Returns nil if there is no
// such destination item e.g. if the group of the WebState at `index` and
// `parentGroup` are distinct groups.
- (TabStripItemIdentifier*)destinationItemAtIndex:(int)index
                                      parentGroup:(const TabGroup*)parentGroup {
  if (!_webStateList->ContainsIndex(index)) {
    return nil;
  }
  web::WebState* destinationWebState = _webStateList->GetWebStateAt(index);

  // Option 1. There is a `parentGroup`.

  if (parentGroup) {
    if (parentGroup->range().contains(index)) {
      // If the item at `index` also belongs to `parentGroup`, then it can be
      // used as a destination item.
      return CreateTabItemIdentifier(destinationWebState);
    }
    // Otherwise `item` cannot be used as a destination item to insert in
    // `parentGroup`.
    return nil;
  }

  // Option 2. There is no `parentGroup`.

  const TabGroup* groupOfDestinationWebState =
      _webStateList->GetGroupOfWebStateAt(index);
  if (groupOfDestinationWebState) {
    // If `item` does belongs to a group, then that group can be used as
    // destination item.
    return CreateGroupItemIdentifier(groupOfDestinationWebState, _webStateList);
  }

  // Otherwise if `item` also does not belong to a group, then it can be used as
  // a destination item.
  return CreateTabItemIdentifier(destinationWebState);
}

// Updates the tab strip items to reflect the move of `webState` in the
// WebStateList. This consists in moving the associated item to its appropriate
// new group (if any) and location, as well as updating the data associated with
// items in `oldGroup` and `newGroup`.
- (void)moveItemForWebState:(web::WebState*)webState
      fromWebStateListIndex:(int)fromIndex
        toWebStateListIndex:(int)toIndex
                   oldGroup:(const TabGroup*)oldGroup
                   newGroup:(const TabGroup*)newGroup {
  TabStripItemIdentifier* itemIdentifier = CreateTabItemIdentifier(webState);

  // Update item data.
  bool itemIsVisible = !newGroup || !newGroup->visual_data().is_collapsed();
  if (itemIsVisible) {
    TabStripItemData* itemData = CreateTabItemData(toIndex, _webStateList);
    [self.consumer updateItemData:@{itemIdentifier : itemData}
                 reconfigureItems:YES];
  }

  // Move item to new position.
  if (fromIndex != toIndex || oldGroup != newGroup) {
    if (TabStripItemIdentifier* previousItemIdentifier =
            [self destinationItemAtIndex:(toIndex - 1) parentGroup:newGroup]) {
      [self.consumer moveItem:itemIdentifier afterItem:previousItemIdentifier];
    } else if (TabStripItemIdentifier* nextItemIdentifier =
                   [self destinationItemAtIndex:(toIndex + 1)
                                    parentGroup:newGroup]) {
      [self.consumer moveItem:itemIdentifier beforeItem:nextItemIdentifier];
    } else if (newGroup) {
      TabGroupItem* newGroupItem =
          [[TabGroupItem alloc] initWithTabGroup:newGroup
                                    webStateList:_webStateList];
      [self.consumer moveItem:itemIdentifier insideGroup:newGroupItem];
    } else {
      [self.consumer moveItem:itemIdentifier beforeItem:nil];
    }
  }

  // Reconfigure the old and new group items if needed.
  if (oldGroup) {
    [self updateDataAndReconfigureItemsInGroup:oldGroup];
  }
  if (newGroup) {
    [self updateDataAndReconfigureItemsInGroup:newGroup];
  }
}

// Inserts and activate a new WebState opened at `kChromeUINewTabURL` using
// `insertionParams`.
- (void)insertAndActivateNewWebStateWithInsertionParams:
    (WebStateList::InsertionParams)insertionParams {
  CHECK(self.profile);
  CHECK(self.webStateList);

  if (!IsAddNewTabAllowedByPolicy(self.profile->GetPrefs(),
                                  self.profile->IsOffTheRecord())) {
    return;
  }

  web::WebState::CreateParams params(self.profile);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  GURL url(kChromeUINewTabURL);
  web::NavigationManager::WebLoadParams loadParams(url);
  loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  self.webStateList->InsertWebState(std::move(webState),
                                    insertionParams.Activate());
}

// Returns whether the WebState at `index` is collapsed i.e. it is inside of a
// collapsed TabGroup.
- (BOOL)webStateIsCollapsedAtIndex:(int)index {
  if (!self.webStateList->ContainsIndex(index)) {
    return NO;
  }
  const TabGroup* groupAtIndex = self.webStateList->GetGroupOfWebStateAt(index);
  return groupAtIndex && groupAtIndex->visual_data().is_collapsed();
}

// Activates a non-collapsed WebState close to the current active WebState. If
// all WebStates in the WebStateList are collapsed, inserts a new WebState and
// activates it.
- (void)activateExistingOrNewNonCollapsedWebState {
  int activeIndex = self.webStateList->active_index();
  if (!self.webStateList->ContainsIndex(activeIndex)) {
    return;
  }
  // If the active WebState is not collapsed, there is nothing to do.
  if (![self webStateIsCollapsedAtIndex:activeIndex]) {
    return;
  }
  // If the active WebState is collapsed, find the closest WebState that is not
  // collapsed.
  const int indexOfNewActiveWebState =
      [self indexOfNonCollapsedWebStateCloseToCollapsedWebStateAt:activeIndex];
  if (self.webStateList->ContainsIndex(indexOfNewActiveWebState)) {
    // If there is a WebState that is not collapsed, activate that WebState.
    self.webStateList->ActivateWebStateAt(indexOfNewActiveWebState);
    return;
  }
  // If there is no WebState to activate on the right or on the left, insert
  // and activate a new WebState at the end of the WebStateList instead.
  const auto insertionParams = WebStateList::InsertionParams::Automatic();
  [self insertAndActivateNewWebStateWithInsertionParams:insertionParams];
}

// Returns the index of a non-collapsed WebState close to `index`. If all
// WebStates in the WebStateList are collapsed, returns `kInvalidIndex`.
- (int)indexOfNonCollapsedWebStateCloseToCollapsedWebStateAt:(int)index {
  CHECK([self webStateIsCollapsedAtIndex:index]);
  // If the tab for WebState at `index` is collapsed, then it must be in a group
  // that is collapsed.
  CHECK(self.webStateList->ContainsIndex(index), base::NotFatalUntil::M128);
  const TabGroup* groupAtIndex = self.webStateList->GetGroupOfWebStateAt(index);
  CHECK(groupAtIndex);
  CHECK(groupAtIndex->visual_data().is_collapsed());
  // If the WebState at `index` is collapsed, find the first WebState to the
  // right of `index` that is not collapsed.
  int newActiveIndex = groupAtIndex->range().range_end();
  while (self.webStateList->ContainsIndex(newActiveIndex)) {
    if (![self webStateIsCollapsedAtIndex:newActiveIndex]) {
      return newActiveIndex;
    }
    newActiveIndex++;
  }
  // If all WebStates to the right of `index` are collapsed, find the first
  // WebState to the left of `index` that is not collapsed.
  newActiveIndex = groupAtIndex->range().range_begin() - 1;
  while (self.webStateList->ContainsIndex(newActiveIndex)) {
    if (![self webStateIsCollapsedAtIndex:newActiveIndex]) {
      return newActiveIndex;
    }
    newActiveIndex--;
  }
  // If all WebStates in the WebStateList are collapsed, return
  // `WebStateList::kInvalidIndex`.
  return WebStateList::kInvalidIndex;
}

@end
