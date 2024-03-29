// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/selected_grid_items.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/web_state.h"

@implementation SelectedGridItems {
  WebStateList* _webStateList;
  std::set<web::WebStateID> _sharableItemsIDs;
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

- (void)addItem:(GridItemIdentifier*)item {
  if ([self containItem:item]) {
    return;
  }
  switch (item.type) {
    case GridItemType::Tab: {
      [_itemsIdentifiers addObject:item];
      web::WebStateID webStateID = item.tabSwitcherItem.identifier;
      if ([self isItemWithIDShareable:webStateID]) {
        _sharableItemsIDs.insert(webStateID);
      }
      _tabsCount += 1;
      return;
    }
    case GridItemType::Group: {
      [_itemsIdentifiers addObject:item];
      const TabGroup* group = item.tabGroupItem.tabGroup;
      WebStateList::Range range = _webStateList->GetGroupRange(group);
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
    case GridItemType::SuggestedActions:
      NOTREACHED();
  }
}

- (void)removeItem:(GridItemIdentifier*)item {
  switch (item.type) {
    case GridItemType::Tab: {
      [_itemsIdentifiers removeObject:item];
      _sharableItemsIDs.erase(item.tabSwitcherItem.identifier);
      _tabsCount -= 1;
      return;
    }
    case GridItemType::Group: {
      const TabGroup* group = item.tabGroupItem.tabGroup;
      WebStateList::Range range = _webStateList->GetGroupRange(group);
      for (int i : range) {
        web::WebStateID webStateID =
            _webStateList->GetWebStateAt(i)->GetUniqueIdentifier();
        _sharableItemsIDs.erase(webStateID);
      }
      [_itemsIdentifiers removeObject:item];
      _tabsCount -= range.count();
      return;
    }
    case GridItemType::SuggestedActions:
      NOTREACHED();
  }
}

- (void)removeAllItems {
  [_itemsIdentifiers removeAllObjects];
  _sharableItemsIDs.clear();
  _tabsCount = 0;
}

- (BOOL)containItem:(GridItemIdentifier*)item {
  CHECK(item.type == GridItemType::Tab || item.type == GridItemType::Group);
  return [_itemsIdentifiers containsObject:item];
}

- (NSUInteger)sharableItemsCount {
  return _sharableItemsIDs.size();
}

- (const std::set<web::WebStateID>&)sharableItems {
  return _sharableItemsIDs;
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
