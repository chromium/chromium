// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/tab_context_menu_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/web/public/web_state.h"

using PinnedState = WebStateSearchCriteria::PinnedState;

@interface TabContextMenuHelper ()

@property(nonatomic, weak) id<TabContextMenuDelegate> contextMenuDelegate;
@property(nonatomic, assign) BOOL incognito;

@end

@implementation TabContextMenuHelper

#pragma mark - TabContextMenuProvider

- (instancetype)initWithProfile:(ProfileIOS*)profile
         tabContextMenuDelegate:
             (id<TabContextMenuDelegate>)tabContextMenuDelegate {
  self = [super init];
  if (self) {
    _profile = profile;
    _contextMenuDelegate = tabContextMenuDelegate;
    _incognito = _profile->IsOffTheRecord();
  }
  return self;
}

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForTabCell:(TabCell*)cell
                          menuScenario:(MenuScenarioHistogram)scenario {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        TabContextMenuHelper* strongSelf = weakSelf;
        if (!strongSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        NSArray<UIMenuElement*>* menuElements =
            [strongSelf menuElementsForTabCell:cell menuScenario:scenario];
        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForTabGroupCell:(TabCell*)cell
                               menuScenario:(MenuScenarioHistogram)scenario {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        TabContextMenuHelper* strongSelf = weakSelf;
        if (!strongSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        NSArray<UIMenuElement*>* menuElements =
            [strongSelf menuElementsForTabGroupCell:cell menuScenario:scenario];
        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (NSArray<UIMenuElement*>*)menuElementsForTabCell:(TabCell*)cell
                                      menuScenario:
                                          (MenuScenarioHistogram)scenario {
  CHECK(cell.itemIdentifier.type == GridItemType::kTab);
  // Record that this context menu was shown to the user.
  RecordMenuShown(scenario);

  web::WebStateID tabID = cell.itemIdentifier.tabSwitcherItem.identifier;

  __weak __typeof(self) weakSelf = self;

  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:scenario];
  const BOOL pinned = IsPinnedTabsEnabled() &&
                      [self isTabPinnedForIdentifier:cell.itemIdentifier];
  const BOOL tabSearchScenario =
      scenario == kMenuScenarioHistogramTabGridSearchResult;
  const BOOL inactive = scenario == kMenuScenarioHistogramInactiveTabsEntry;

  TabItem* item = [self tabItemForIdentifier:tabID];

  if (!item) {
    return @[];
  }

  UIMenuElement* pinAction;
  UIMenuElement* shareAction;
  UIMenuElement* addToReadingListAction;
  UIAction* bookmarkAction;
  UIMenuElement* selectAction;
  UIAction* closeTabAction;

  const BOOL isPinActionEnabled = IsPinnedTabsEnabled() && !self.incognito &&
                                  !inactive && !tabSearchScenario;
  if (isPinActionEnabled) {
    if (pinned) {
      pinAction = [actionFactory actionToUnpinTabWithBlock:^{
        [self.contextMenuDelegate unpinTabWithIdentifier:tabID];
      }];
    } else {
      pinAction = [actionFactory actionToPinTabWithBlock:^{
        [self.contextMenuDelegate pinTabWithIdentifier:tabID];
      }];
    }
  }

  if (!IsURLNewTabPage(item.URL)) {
    shareAction = [actionFactory actionToShareWithBlock:^{
      [self.contextMenuDelegate shareURL:item.URL
                                   title:item.title
                                scenario:SharingScenario::TabGridItem
                                fromView:cell];
    }];

    if (item.URL.SchemeIsHTTPOrHTTPS()) {
      addToReadingListAction =
          [actionFactory actionToAddToReadingListWithBlock:^{
            [self.contextMenuDelegate addToReadingListURL:item.URL
                                                    title:item.title];
          }];
    }

    if (_profile) {
      const BOOL currentlyBookmarked = [self isTabItemBookmarked:item];
      if (currentlyBookmarked) {
        bookmarkAction = [actionFactory actionToEditBookmarkWithBlock:^{
          [self.contextMenuDelegate editBookmarkWithURL:item.URL];
        }];
      } else {
        bookmarkAction = [actionFactory actionToBookmarkWithBlock:^{
          [self.contextMenuDelegate bookmarkURL:item.URL title:item.title];
        }];
      }
      // Bookmarking can be disabled from prefs (from an enterprise policy),
      // if that's the case grey out the option in the menu.
      BOOL isEditBookmarksEnabled = _profile->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled);
      if (!isEditBookmarksEnabled && bookmarkAction) {
        bookmarkAction.attributes = UIMenuElementAttributesDisabled;
      }
    }
  }

  // Pinned tabs, inactive tabs and search results menus don't
  // support tab selection.
  BOOL scenarioDisablesSelection =
      scenario == kMenuScenarioHistogramTabGridSearchResult ||
      scenario == kMenuScenarioHistogramPinnedTabsEntry ||
      scenario == kMenuScenarioHistogramInactiveTabsEntry;
  if (!scenarioDisablesSelection) {
    selectAction = [actionFactory actionToSelectTabsWithBlock:^{
      [self.contextMenuDelegate selectTabs];
    }];
  }

  ProceduralBlock closeTabActionBlock = ^{
    [self.contextMenuDelegate closeTabWithIdentifier:tabID
                                           incognito:self.incognito];
  };

  if (IsPinnedTabsEnabled() && !self.incognito && pinned) {
    closeTabAction =
        [actionFactory actionToClosePinnedTabWithBlock:closeTabActionBlock];
  } else {
    closeTabAction =
        [actionFactory actionToCloseRegularTabWithBlock:closeTabActionBlock];
  }

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  if (IsTabGroupInGridEnabled()) {
    std::set<const TabGroup*> groups = GetAllGroupsForProfile(_profile);

    auto actionResult = ^(const TabGroup* group) {
      [weakSelf handleAddWebState:tabID toGroup:group];
    };

    const TabGroup* currentTabGroup = [self groupForWebState:tabID];
    UIMenuElement* groupAction;
    if (currentTabGroup) {
      ProceduralBlock removeBlock = ^{
        [weakSelf handleRemoveWebStateFromGroup:tabID];
      };
      groupAction =
          [actionFactory menuToMoveTabToGroupWithGroups:groups
                                           currentGroup:currentTabGroup
                                              moveBlock:actionResult
                                            removeBlock:removeBlock];
    } else {
      groupAction = [actionFactory menuToAddTabToGroupWithGroups:groups
                                                    numberOfTabs:1
                                                           block:actionResult];
    }

    // Hide the `shareAction` for tabs in groups.
    if (shareAction && !currentTabGroup) {
      UIMenu* shareMenu = [UIMenu menuWithTitle:@""
                                          image:nil
                                     identifier:nil
                                        options:UIMenuOptionsDisplayInline
                                       children:@[ shareAction ]];
      [menuElements addObject:shareMenu];
    }
    NSArray<UIMenuElement*>* tabActions =
        pinAction ? @[ pinAction, groupAction ] : @[ groupAction ];
    UIMenu* tabMenu = [UIMenu menuWithTitle:@""
                                      image:nil
                                 identifier:nil
                                    options:UIMenuOptionsDisplayInline
                                   children:tabActions];
    [menuElements addObject:tabMenu];

    NSMutableArray<UIMenuElement*>* collectionsActions = [NSMutableArray array];
    if (addToReadingListAction) {
      [collectionsActions addObject:addToReadingListAction];
    }
    if (bookmarkAction) {
      [collectionsActions addObject:bookmarkAction];
    }
    // Hide the `selectAction` for tabs in groups.
    if (selectAction && !currentTabGroup) {
      [collectionsActions addObject:selectAction];
    }
    if (closeTabAction) {
      [collectionsActions addObject:closeTabAction];
    }

    if (collectionsActions.count > 0) {
      UIMenu* collectionsMenu = [UIMenu menuWithTitle:@""
                                                image:nil
                                           identifier:nil
                                              options:UIMenuOptionsDisplayInline
                                             children:collectionsActions];
      [menuElements addObject:collectionsMenu];
    }

  } else {
    if (pinAction) {
      [menuElements addObject:pinAction];
    }
    if (shareAction) {
      [menuElements addObject:shareAction];
    }
    if (addToReadingListAction) {
      [menuElements addObject:addToReadingListAction];
    }
    if (bookmarkAction) {
      [menuElements addObject:bookmarkAction];
    }
    if (selectAction) {
      [menuElements addObject:selectAction];
    }
    if (closeTabAction) {
      [menuElements addObject:closeTabAction];
    }
  }

  return menuElements;
}

- (NSArray<UIMenuElement*>*)menuElementsForTabGroupCell:(TabCell*)cell
                                           menuScenario:
                                               (MenuScenarioHistogram)scenario {
  CHECK(cell.itemIdentifier.type == GridItemType::kGroup);
  // Record that this context menu was shown to the user.
  RecordMenuShown(scenario);

  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:scenario];

  const TabGroup* group = cell.itemIdentifier.tabGroupItem.tabGroup;
  base::WeakPtr<const TabGroup> weakGroup = group->GetWeakPtr();
  BOOL incognito = self.incognito;
  CHECK(group);
  __weak __typeof(self) weakSelf = self;

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  [menuElements addObject:[actionFactory actionToRenameTabGroupWithBlock:^{
                  [weakSelf.contextMenuDelegate editTabGroup:weakGroup
                                                   incognito:incognito];
                }]];
  [menuElements addObject:[actionFactory actionToUngroupTabGroupWithBlock:^{
                  [weakSelf.contextMenuDelegate ungroupTabGroup:weakGroup
                                                      incognito:incognito
                                                     sourceView:cell];
                }]];

  if (IsTabGroupSyncEnabled()) {
    [menuElements addObject:[actionFactory actionToCloseTabGroupWithBlock:^{
                    [weakSelf.contextMenuDelegate closeTabGroup:weakGroup
                                                      incognito:incognito];
                  }]];
    if (!incognito) {
      [menuElements addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                      [weakSelf.contextMenuDelegate deleteTabGroup:weakGroup
                                                         incognito:incognito
                                                        sourceView:cell];
                    }]];
    }
  } else {
    [menuElements addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                    [weakSelf.contextMenuDelegate deleteTabGroup:weakGroup
                                                       incognito:incognito
                                                      sourceView:cell];
                  }]];
  }

  return menuElements;
}

#pragma mark - Private

// Returns `YES` if the tab `item` is already bookmarked.
- (BOOL)isTabItemBookmarked:(TabItem*)item {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForProfile(_profile);
  return item && bookmarkModel->IsBookmarked(item.URL);
}

// Returns `YES` if the tab for the given `identifier` is pinned.
- (BOOL)isTabPinnedForIdentifier:(GridItemIdentifier*)identifier {
  if (!identifier || (identifier.type != GridItemType::kTab)) {
    return NO;
  }

  BrowserList* browserList = BrowserListFactory::GetForProfile(_profile);

  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    WebStateList* webStateList = browser->GetWebStateList();
    web::WebState* webState = GetWebState(
        webStateList, WebStateSearchCriteria{
                          .identifier = identifier.tabSwitcherItem.identifier,
                          .pinned_state = PinnedState::kPinned,
                      });
    if (webState) {
      return YES;
    }
  }
  return NO;
}

// Returns the TabItem object representing the tab with `identifier.
- (TabItem*)tabItemForIdentifier:(web::WebStateID)identifier {
  BrowserList* browserList = BrowserListFactory::GetForProfile(_profile);
  const BrowserList::BrowserType browser_types =
      _incognito ? BrowserList::BrowserType::kIncognito
                 : BrowserList::BrowserType::kRegularAndInactive;
  std::set<Browser*> browsers = browserList->BrowsersOfType(browser_types);
  for (Browser* browser : browsers) {
    WebStateList* webStateList = browser->GetWebStateList();
    TabItem* item = GetTabItem(
        webStateList, WebStateSearchCriteria{.identifier = identifier});
    if (item != nil) {
      return item;
    }
  }
  return nil;
}

// Handles the result of the add to group block.
- (void)handleAddWebState:(web::WebStateID)webStateID
                  toGroup:(const TabGroup*)group {
  if (group == nullptr) {
    [self.contextMenuDelegate createNewTabGroupWithIdentifier:webStateID
                                                    incognito:self.incognito];
  } else {
    MoveTabToGroup(webStateID, group, _profile);
  }
}

// Handles the result of the remove from group block.
- (void)handleRemoveWebStateFromGroup:(web::WebStateID)webStateID {
  BrowserList* browserList = BrowserListFactory::GetForProfile(_profile);

  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    WebStateList* webStateList = browser->GetWebStateList();
    int index = GetWebStateIndex(
        webStateList,
        WebStateSearchCriteria{.identifier = webStateID,
                               .pinned_state = PinnedState::kNonPinned});
    if (index != WebStateList::kInvalidIndex) {
      webStateList->RemoveFromGroups({index});
      return;
    }
  }
}

// Returns the group of the given `webStateID`.
- (const TabGroup*)groupForWebState:(web::WebStateID)webStateID {
  BrowserList* browserList = BrowserListFactory::GetForProfile(_profile);

  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    WebStateList* webStateList = browser->GetWebStateList();
    int index = GetWebStateIndex(
        webStateList,
        WebStateSearchCriteria{.identifier = webStateID,
                               .pinned_state = PinnedState::kNonPinned});
    if (webStateList->ContainsIndex(index)) {
      return webStateList->GetGroupOfWebStateAt(index);
    }
  }
  return nil;
}

@end
