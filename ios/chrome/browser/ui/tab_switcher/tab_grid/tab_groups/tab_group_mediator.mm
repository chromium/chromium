// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_mediator.h"

#import <algorithm>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_idle_status_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

@implementation TabGroupMediator {
  // Tab group consumer.
  __weak id<TabGroupConsumer> _groupConsumer;
  // Current group.
  base::WeakPtr<const TabGroup> _tabGroup;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            tabGroup:(base::WeakPtr<const TabGroup>)tabGroup
                            consumer:(id<TabGroupConsumer>)groupConsumer
                        gridConsumer:(id<TabCollectionConsumer>)gridConsumer
                          modeHolder:(TabGridModeHolder*)modeHolder {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group mediator outside the "
         "Tab Groups experiment.";
  CHECK(webStateList);
  CHECK(groupConsumer);
  CHECK(tabGroup);
  if ((self = [super initWithModeHolder:modeHolder])) {
    self.webStateList = webStateList;
    _groupConsumer = groupConsumer;
    self.consumer = gridConsumer;

    _tabGroup = tabGroup;

    [_groupConsumer setGroupTitle:tabGroup->GetTitle()];
    [_groupConsumer setGroupColor:tabGroup->GetColor()];

    [self populateConsumerItems];
  }
  return self;
}

#pragma mark - TabGroupMutator

- (BOOL)addNewItemInGroup {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  if (!_tabGroup) {
    return NO;
  }
  GURL URL(kChromeUINewTabURL);
  int groupCount = _tabGroup->range().count();
  [self insertNewWebStateAtGridIndex:groupCount withURL:URL];
  return groupCount != _tabGroup->range().count();
}

- (void)ungroup {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  auto scoped_lock = self.webStateList->StartBatchOperation();
  self.webStateList->DeleteGroup(_tabGroup.get());
  _tabGroup.reset();
}

- (void)closeGroup {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  if (IsTabGroupSyncEnabled()) {
    tab_groups::TabGroupSyncService* syncService =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(
            self.browser->GetProfile());
    tab_groups::utils::CloseTabGroupLocally(_tabGroup.get(), self.webStateList,
                                            syncService);
  } else {
    CloseAllWebStatesInGroup(*self.webStateList, _tabGroup.get(),
                             WebStateList::CLOSE_USER_ACTION);
  }
  _tabGroup.reset();
}

- (void)deleteGroup {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  CloseAllWebStatesInGroup(*self.webStateList, _tabGroup.get(),
                           WebStateList::CLOSE_USER_ACTION);
  _tabGroup.reset();
}

#pragma mark - Parent's functions

- (void)configureToolbarsButtons {
  // No-op
}

// Overrides the parent to only display tabs from the group.
- (void)populateConsumerItems {
  if (!self.webStateList || !_tabGroup) {
    return;
  }

  GridItemIdentifier* identifier = nil;
  int webStateIndex = self.webStateList->active_index();
  if (webStateIndex != WebStateList::kInvalidIndex &&
      self.webStateList->GetGroupOfWebStateAt(webStateIndex) ==
          _tabGroup.get()) {
    web::WebState* webState = self.webStateList->GetWebStateAt(webStateIndex);
    identifier = [GridItemIdentifier tabIdentifier:webState];
  }

  [self.consumer populateItems:CreateTabItems(self.webStateList,
                                              _tabGroup->range())
        selectedItemIdentifier:identifier];
}

// Override the parent to only show individual web state in the group.
- (GridItemIdentifier*)activeIdentifier {
  WebStateList* webStateList = self.webStateList;
  if (!webStateList || !_tabGroup) {
    return nil;
  }

  int webStateIndex = webStateList->active_index();
  if (webStateIndex == WebStateList::kInvalidIndex) {
    return nil;
  }

  if (!_tabGroup->range().contains(webStateIndex)) {
    return nil;
  }

  return [GridItemIdentifier
      tabIdentifier:webStateList->GetWebStateAt(webStateIndex)];
}

// Overrides the parent observations: only observe the group `WebState`s.
- (void)addWebStateObservations {
  if (!_tabGroup) {
    return;
  }
  for (int index : _tabGroup->range()) {
    web::WebState* webState = self.webStateList->GetWebStateAt(index);
    [self addObservationForWebState:webState];
  }
}

// Overrides the parent as there is only tab cells.
- (void)insertItem:(GridItemIdentifier*)item
    beforeWebStateIndex:(int)nextWebStateIndex {
  GridItemIdentifier* nextItemIdentifier;
  if (nextWebStateIndex < _tabGroup->range().range_end()) {
    nextItemIdentifier = [GridItemIdentifier
        tabIdentifier:self.webStateList->GetWebStateAt(nextWebStateIndex)];
  }
  [self.consumer insertItem:item
                beforeItemID:nextItemIdentifier
      selectedItemIdentifier:[self activeIdentifier]];
}

// Overrides the parent as there is only tab cells.
- (void)moveItem:(GridItemIdentifier*)item
    beforeWebStateIndex:(int)nextWebStateIndex {
  GridItemIdentifier* nextItem;
  if (nextWebStateIndex < _tabGroup->range().range_end()) {
    nextItem = [GridItemIdentifier
        tabIdentifier:self.webStateList->GetWebStateAt(nextWebStateIndex)];
  }
  [self.consumer moveItem:item beforeItem:nextItem];
}

// Overrides the parent as there is only tab cells.
- (void)updateConsumerItemForWebState:(web::WebState*)webState {
  GridItemIdentifier* item = [GridItemIdentifier tabIdentifier:webState];
  [self.consumer replaceItem:item withReplacementItem:item];
}

- (void)insertNewWebStateAtGridIndex:(int)index withURL:(const GURL&)newTabURL {
  ProfileIOS* profile = self.browser->GetProfile();

  if (!self.browser || !_tabGroup || !profile) {
    return;
  }

  WebStateList* webStateList = self.webStateList;
  if (!webStateList->ContainsGroup(_tabGroup.get())) {
    return;
  }

  int webStateListIndex = _tabGroup->range().range_begin() + index;
  webStateListIndex =
      std::clamp(webStateListIndex, _tabGroup->range().range_begin(),
                 _tabGroup->range().range_end());

  UrlLoadParams params = UrlLoadParams::InNewTab(newTabURL);
  params.in_incognito = profile->IsOffTheRecord();
  params.append_to = OpenPosition::kSpecifiedIndex;
  params.insertion_index = webStateListIndex;
  params.load_in_group = true;
  params.tab_group = _tabGroup;
  self.URLLoader->Load(params);
}

- (BOOL)canHandleTabGroupDrop:(TabGroupInfo*)tabGroupInfo {
  return NO;
}

- (void)recordExternalURLDropped {
  base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                DragItemOrigin::kOther);
}

#pragma mark - TabCollectionDragDropHandler override

// Overrides the parent as the given destination index do not take into account
// elements outside the group.
- (void)dropItem:(UIDragItem*)dragItem
               toIndex:(NSUInteger)destinationIndex
    fromSameCollection:(BOOL)fromSameCollection {
  WebStateList* webStateList = self.webStateList;
  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    int destinationWebStateIndex =
        _tabGroup->range().range_begin() + destinationIndex;
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);
    int sourceWebStateIndex =
        GetWebStateIndex(webStateList, WebStateSearchCriteria{
                                           .identifier = tabInfo.tabID,
                                       });
    const auto insertionParams =
        WebStateList::InsertionParams::AtIndex(destinationWebStateIndex)
            .InGroup(_tabGroup.get());
    if (sourceWebStateIndex == WebStateList::kInvalidIndex) {
      base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                    DragItemOrigin::kOtherBrowser);
      MoveTabToBrowser(tabInfo.tabID, self.browser, insertionParams);
      return;
    }

    if (fromSameCollection) {
      base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
    } else {
      base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                    DragItemOrigin::kSameBrowser);
    }

    // Reorder tabs.
    MoveWebStateWithIdentifierToInsertionParams(
        tabInfo.tabID, insertionParams, webStateList, fromSameCollection);
    return;
  }
  base::UmaHistogramEnumeration(kUmaGroupViewDragOrigin,
                                DragItemOrigin::kOther);

  // Handle URLs from within Chrome synchronously using a local object.
  if ([dragItem.localObject isKindOfClass:[URLInfo class]]) {
    URLInfo* droppedURL = static_cast<URLInfo*>(dragItem.localObject);
    [self insertNewWebStateAtGridIndex:destinationIndex withURL:droppedURL.URL];
    return;
  }
}

#pragma mark - WebStateListObserving override

// Overrides the parent observations. The parent treats a group as one cell,
// whereas this TabGroupMediator only cares about one group, and shows grouped
// tabs as many cells.
- (void)willChangeWebStateList:(WebStateList*)webStateList
                        change:(const WebStateListChangeDetach&)detachChange
                        status:(const WebStateListStatus&)status {
  DCHECK_EQ(self.webStateList, webStateList);
  if (webStateList->IsBatchInProgress() || !_tabGroup) {
    return;
  }
  CHECK(detachChange.group() == _tabGroup.get());

  web::WebState* detachedWebState = detachChange.detached_web_state();
  GridItemIdentifier* identifierToRemove =
      [GridItemIdentifier tabIdentifier:detachedWebState];
  [self.consumer removeItemWithIdentifier:identifierToRemove
                   selectedItemIdentifier:[self activeIdentifier]];
  [self removeObservationForWebState:detachedWebState];
}

// Overrides the parent observations. The parent treats a group as one cell and
// just update it, whereas this TabGroupMediator treats them as multiples cell,
// so this overrides manages notifications accordingly.
- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(self.webStateList, webStateList);
  if (webStateList->IsBatchInProgress() || !_tabGroup) {
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly: {
      const WebStateListChangeStatusOnly& selectionOnlyChange =
          change.As<WebStateListChangeStatusOnly>();
      const TabGroup* oldGroup = selectionOnlyChange.old_group();
      const TabGroup* newGroup = selectionOnlyChange.new_group();

      if (oldGroup != newGroup) {
        // There is a change of group.
        if (oldGroup == _tabGroup.get()) {
          web::WebState* currentWebState =
              self.webStateList->GetWebStateAt(selectionOnlyChange.index());

          GridItemIdentifier* tabIdentifierToAddToGroup =
              [GridItemIdentifier tabIdentifier:currentWebState];
          [self.consumer removeItemWithIdentifier:tabIdentifierToAddToGroup
                           selectedItemIdentifier:[self activeIdentifier]];
          [self removeObservationForWebState:currentWebState];
        }

        if (newGroup == _tabGroup.get()) {
          int webStateIndex = selectionOnlyChange.index();
          web::WebState* currentWebState =
              self.webStateList->GetWebStateAt(webStateIndex);

          [self insertItem:[GridItemIdentifier tabIdentifier:currentWebState]
              beforeWebStateIndex:webStateIndex + 1];
          [self addObservationForWebState:currentWebState];
        }
        break;
      }
      break;
    }
    case WebStateListChange::Type::kGroupVisualDataUpdate: {
      const WebStateListChangeGroupVisualDataUpdate& visualDataChange =
          change.As<WebStateListChangeGroupVisualDataUpdate>();
      const TabGroup* tabGroup = visualDataChange.updated_group();
      if (_tabGroup.get() != tabGroup) {
        break;
      }
      [_groupConsumer setGroupTitle:tabGroup->GetTitle()];
      [_groupConsumer setGroupColor:tabGroup->GetColor()];
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& groupDeleteChange =
          change.As<WebStateListChangeGroupDelete>();
      if (groupDeleteChange.deleted_group() == _tabGroup.get()) {
        _tabGroup.reset();
        [self.tabGroupsHandler hideTabGroup];
      }
      break;
    }
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& moveChange =
          change.As<WebStateListChangeMove>();
      if (moveChange.old_group() != _tabGroup.get() &&
          moveChange.new_group() != _tabGroup.get()) {
        // Not related to this group.
        break;
      }
      web::WebState* webState = moveChange.moved_web_state();
      GridItemIdentifier* item = [GridItemIdentifier tabIdentifier:webState];
      if (moveChange.old_group() == moveChange.new_group()) {
        // Move in the same group
        [self moveItem:item
            beforeWebStateIndex:moveChange.moved_to_index() + 1];
      } else {
        if (moveChange.old_group() == _tabGroup.get()) {
          // The tab left the group.
          [self.consumer removeItemWithIdentifier:item
                           selectedItemIdentifier:[self activeIdentifier]];
          [self removeObservationForWebState:webState];
        } else if (moveChange.new_group() == _tabGroup.get()) {
          // The tab joined the group.
          [self insertInConsumerWebState:webState
                                 atIndex:moveChange.moved_to_index()];
          [self addObservationForWebState:webState];
        }
      }
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insertChange =
          change.As<WebStateListChangeInsert>();
      if (insertChange.group() != _tabGroup.get()) {
        break;
      }

      [self insertInConsumerWebState:insertChange.inserted_web_state()
                             atIndex:insertChange.index()];

      [self addObservationForWebState:insertChange.inserted_web_state()];
      break;
    }
    default:
      [super didChangeWebStateList:webStateList change:change status:status];
      break;
  }
  if (_tabGroup) {
    // Update the title in case the number of tabs changed.
    [_groupConsumer setGroupTitle:_tabGroup->GetTitle()];
  }
  if (status.active_web_state_change()) {
    [self.consumer selectItemWithIdentifier:[self activeIdentifier]];
  }
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(self.webStateList, webStateList);
  [self addWebStateObservations];
  [self populateConsumerItems];
  if (_tabGroup) {
    [_groupConsumer setGroupTitle:_tabGroup->GetTitle()];
    [_groupConsumer setGroupColor:_tabGroup->GetColor()];
  } else {
    [self.tabGroupsHandler hideTabGroup];
  }
}

#pragma mark - Private

// Inserts an item representing `webState` in the consumer at `index`.
- (void)insertInConsumerWebState:(web::WebState*)webState atIndex:(int)index {
  CHECK(_tabGroup);
  GridItemIdentifier* newItem = [GridItemIdentifier tabIdentifier:webState];

  GridItemIdentifier* nextItemIdentifier;
  if (index + 1 < _tabGroup->range().range_end()) {
    nextItemIdentifier = [GridItemIdentifier
        tabIdentifier:self.webStateList->GetWebStateAt(index + 1)];
  }

  [self.consumer insertItem:newItem
                beforeItemID:nextItemIdentifier
      selectedItemIdentifier:[self activeIdentifier]];
}

@end
