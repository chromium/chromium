// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/selected_grid_items.h"

#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/web_state.h"

@implementation SelectedGridItems {
  raw_ptr<WebStateList> _webStateList;
  std::set<web::WebStateID> _sharableItemsIDs;
  NSMutableSet<GridItemIdentifier*>* _itemsIdentifiers;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  CHECK(webStateList);
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _itemsIdentifiers = [NSMutableSet set];
  }
  return self;
}

- (NSSet<GridItemIdentifier*>*)itemsIdentifiers {
  return _itemsIdentifiers;
}

- (void)addItem:(GridItemIdentifier*)item {
  if ([self containItem:item]) {
    return;
  }
  switch (item.type) {
    case GridItemType::kInactiveTabsButton:
      NOTREACHED();
    case GridItemType::kTab: {
      [_itemsIdentifiers addObject:item];
      web::WebStateID webStateID = item.tabSwitcherItem.identifier;
      if ([self isItemWithIDShareable:webStateID]) {
        _sharableItemsIDs.insert(webStateID);
      }
      _tabsCount += 1;
      return;
    }
    case GridItemType::kGroup: {
      [_itemsIdentifiers addObject:item];
      const TabGroup* group = item.tabGroupItem.tabGroup;
      const TabGroupRange range = group->range();
      for (int i : range) {
        web::WebStateID webStateID =
            _webStateList->GetWebStateAt(i)->GetUniqueIdentifier();
        if ([self isItemWithIDShareable:webStateID]) {
          _sharableItemsIDs.insert(webStateID);
        }
      }
      _tabsCount += range.count();
      return;
    }
    case GridItemType::kSuggestedActions:
      NOTREACHED();
  }
}

- (void)removeItem:(GridItemIdentifier*)item {
  switch (item.type) {
    case GridItemType::kInactiveTabsButton:
      NOTREACHED();
    case GridItemType::kTab: {
      [_itemsIdentifiers removeObject:item];
      _sharableItemsIDs.erase(item.tabSwitcherItem.identifier);
      _tabsCount -= 1;
      return;
    }
    case GridItemType::kGroup: {
      const TabGroup* group = item.tabGroupItem.tabGroup;
      const TabGroupRange range = group->range();
      for (int i : range) {
        web::WebStateID webStateID =
            _webStateList->GetWebStateAt(i)->GetUniqueIdentifier();
        _sharableItemsIDs.erase(webStateID);
      }
      [_itemsIdentifiers removeObject:item];
      _tabsCount -= range.count();
      return;
    }
    case GridItemType::kSuggestedActions:
      NOTREACHED();
  }
}

- (void)removeAllItems {
  [_itemsIdentifiers removeAllObjects];
  _sharableItemsIDs.clear();
  _tabsCount = 0;
}

- (BOOL)containItem:(GridItemIdentifier*)item {
  CHECK(item.type == GridItemType::kTab || item.type == GridItemType::kGroup);
  return [_itemsIdentifiers containsObject:item];
}

- (NSUInteger)sharableTabsCount {
  return _sharableItemsIDs.size();
}

- (const std::set<web::WebStateID>&)sharableTabs {
  return _sharableItemsIDs;
}

- (std::set<web::WebStateID>)allTabs {
  std::set<web::WebStateID> tabs;
  for (GridItemIdentifier* item in _itemsIdentifiers) {
    switch (item.type) {
      case GridItemType::kInactiveTabsButton:
        NOTREACHED();
      case GridItemType::kTab:
        tabs.insert(item.tabSwitcherItem.identifier);
        break;
      case GridItemType::kGroup: {
        CHECK(item.tabGroupItem.tabGroup);
        for (int i : item.tabGroupItem.tabGroup->range()) {
          tabs.insert(_webStateList->GetWebStateAt(i)->GetUniqueIdentifier());
        }
        break;
      }
      case GridItemType::kSuggestedActions:
        NOTREACHED();
    }
  }
  return tabs;
}

- (NSArray<URLWithTitle*>*)selectedTabsURLs {
  NSMutableArray<URLWithTitle*>* URLs = [[NSMutableArray alloc] init];
  for (const web::WebStateID itemID : _sharableItemsIDs) {
    TabItem* item = GetTabItem(
        _webStateList,
        WebStateSearchCriteria{
            .identifier = itemID,
            .pinned_state = WebStateSearchCriteria::PinnedState::kNonPinned,
        });
    URLWithTitle* URL = [[URLWithTitle alloc] initWithURL:item.URL
                                                    title:item.title];
    [URLs addObject:URL];
  }
  return URLs;
}

#pragma mark - Private

// Returns YES if the provided webState can be shared.
- (BOOL)isItemWithIDShareable:(web::WebStateID)itemID {
  web::WebState* webState = GetWebState(
      _webStateList,
      WebStateSearchCriteria{
          .identifier = itemID,
          .pinned_state = WebStateSearchCriteria::PinnedState::kNonPinned,
      });
  const GURL& URL = webState->GetVisibleURL();
  return URL.is_valid() && URL.SchemeIsHTTPOrHTTPS();
}

@end
