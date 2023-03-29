// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"

#import "base/metrics/histogram_functions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/ntp/new_tab_page_util.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/tab_context_menu_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using PinnedState = WebStateSearchCriteria::PinnedState;

@interface TabContextMenuHelper ()

@property(nonatomic, weak) id<TabContextMenuDelegate> contextMenuDelegate;
@property(nonatomic, assign) BOOL incognito;

@end

@implementation TabContextMenuHelper

#pragma mark - TabContextMenuProvider

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
              tabContextMenuDelegate:
                  (id<TabContextMenuDelegate>)tabContextMenuDelegate {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _contextMenuDelegate = tabContextMenuDelegate;
    _incognito = _browserState->IsOffTheRecord();
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

- (NSArray<UIMenuElement*>*)menuElementsForTabCell:(TabCell*)cell
                                      menuScenario:
                                          (MenuScenarioHistogram)scenario {
  // Record that this context menu was shown to the user.
  RecordMenuShown(scenario);

  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:scenario];
  const BOOL pinned = IsPinnedTabsEnabled() &&
                      [self isTabPinnedForIdentifier:cell.itemIdentifier];
  const BOOL tabSearchScenario =
      scenario == MenuScenarioHistogram::kTabGridSearchResult;
  const BOOL inactive = scenario == MenuScenarioHistogram::kInactiveTabsEntry;

  TabItem* item = [self tabItemForIdentifier:cell.itemIdentifier];

  if (!item) {
    return @[];
  }

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  const BOOL isPinActionEnabled = IsPinnedTabsEnabled() && !self.incognito &&
                                  !inactive && !tabSearchScenario;
  if (isPinActionEnabled) {
    if (pinned) {
      [menuElements addObject:[actionFactory actionToUnpinTabWithBlock:^{
                      [self.contextMenuDelegate
                          unpinTabWithIdentifier:cell.itemIdentifier];
                    }]];
    } else {
      [menuElements addObject:[actionFactory actionToPinTabWithBlock:^{
                      [self.contextMenuDelegate
                          pinTabWithIdentifier:cell.itemIdentifier];
                    }]];
    }
  }

  if (!IsURLNewTabPage(item.URL)) {
    [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                    [self.contextMenuDelegate
                        shareURL:item.URL
                           title:item.title
                        scenario:SharingScenario::TabGridItem
                        fromView:cell];
                  }]];

    if (item.URL.SchemeIsHTTPOrHTTPS()) {
      [menuElements
          addObject:[actionFactory actionToAddToReadingListWithBlock:^{
            [self.contextMenuDelegate addToReadingListURL:item.URL
                                                    title:item.title];
          }]];
    }

    UIAction* bookmarkAction;
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
    if (_browserState) {
      BOOL isEditBookmarksEnabled = _browserState->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled);
      if (!isEditBookmarksEnabled && bookmarkAction) {
        bookmarkAction.attributes = UIMenuElementAttributesDisabled;
      }
      if (bookmarkAction) {
        [menuElements addObject:bookmarkAction];
      }
    }
  }

  // Thumb strip, pinned tabs, inactive tabs and search results menus don't
  // support tab selection.
  BOOL scenarioDisablesSelection =
      scenario == MenuScenarioHistogram::kTabGridSearchResult ||
      scenario == MenuScenarioHistogram::kPinnedTabsEntry ||
      scenario == MenuScenarioHistogram::kInactiveTabsEntry ||
      scenario == MenuScenarioHistogram::kThumbStrip;
  if (!scenarioDisablesSelection) {
    [menuElements addObject:[actionFactory actionToSelectTabsWithBlock:^{
                    [self.contextMenuDelegate selectTabs];
                  }]];
  }

  UIAction* closeTabAction;
  ProceduralBlock closeTabActionBlock = ^{
    [self.contextMenuDelegate closeTabWithIdentifier:cell.itemIdentifier
                                           incognito:self.incognito
                                              pinned:pinned];
  };

  if (IsPinnedTabsEnabled() && !self.incognito && pinned) {
    closeTabAction =
        [actionFactory actionToClosePinnedTabWithBlock:closeTabActionBlock];
  } else {
    closeTabAction =
        [actionFactory actionToCloseRegularTabWithBlock:closeTabActionBlock];
  }

  [menuElements addObject:closeTabAction];

  return menuElements;
}

#pragma mark - Private

// Returns `YES` if the tab `item` is already bookmarked.
- (BOOL)isTabItemBookmarked:(TabItem*)item {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          _browserState);
  return item && bookmarkModel &&
         bookmarkModel->GetMostRecentlyAddedUserNodeForURL(item.URL);
}

// Returns `YES` if the tab for the given `identifier` is pinned.
- (BOOL)isTabPinnedForIdentifier:(NSString*)identifier {
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(_browserState);

  for (Browser* browser : browserList->AllRegularBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    web::WebState* webState =
        GetWebState(webStateList, WebStateSearchCriteria{
                                      .identifier = identifier,
                                      .pinned_state = PinnedState::kPinned,
                                  });
    if (webState) {
      return YES;
    }
  }
  return NO;
}

// Returns the TabItem object representing the tab with `identifier.
- (TabItem*)tabItemForIdentifier:(NSString*)identifier {
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(_browserState);
  std::set<Browser*> browsers = _incognito ? browserList->AllIncognitoBrowsers()
                                           : browserList->AllRegularBrowsers();
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

@end
