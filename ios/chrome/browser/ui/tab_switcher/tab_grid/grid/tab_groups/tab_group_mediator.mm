// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/navigation/navigation_manager.h"
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
                        gridConsumer:(id<TabCollectionConsumer>)gridConsumer {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group mediator outside the "
         "Tab Groups experiment.";
  CHECK(webStateList);
  CHECK(groupConsumer);
  CHECK(tabGroup);
  if (self = [super init]) {
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
  return [self addTabToGroup:_tabGroup.get()];
}

- (void)ungroup {
  auto scoped_lock = self.webStateList->StartBatchOperation();
  self.webStateList->DeleteGroup(_tabGroup.get());
  _tabGroup.reset();
}

- (void)deleteGroup {
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

#pragma mark - WebStateListObserving override

// Overrides the parent observations. The parent treats a group as one cell,
// whereas this TabGroupMediator only cares about one group, and shows grouped
// tabs as many cells.
- (void)willChangeWebStateList:(WebStateList*)webStateList
                        change:(const WebStateListChangeDetach&)detachChange
                        status:(const WebStateListStatus&)status {
  DCHECK_EQ(self.webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }
  CHECK(detachChange.group() == _tabGroup.get());

  web::WebState* detachedWebState = detachChange.detached_web_state();
  GridItemIdentifier* identifierToRemove =
      [GridItemIdentifier tabIdentifier:detachedWebState];
  GridItemIdentifier* selectedIdentifier;
  if (self.webStateList->GetGroupOfWebStateAt(webStateList->active_index()) ==
      _tabGroup.get()) {
    selectedIdentifier =
        [GridItemIdentifier tabIdentifier:webStateList->GetActiveWebState()];
  }
  [self.consumer removeItemWithIdentifier:identifierToRemove
                   selectedItemIdentifier:selectedIdentifier];
  [self removeObservationForWebState:detachedWebState];
}

// Overrides the parent observations. The parent treats a group as one cell and
// just update it, whereas this TabGroupMediator treats them as multiples cell,
// so this overrides manages notifications accordingly.
- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(self.webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  switch (change.type()) {
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
    default:
      [super didChangeWebStateList:webStateList change:change status:status];
      break;
  }
}

@end
