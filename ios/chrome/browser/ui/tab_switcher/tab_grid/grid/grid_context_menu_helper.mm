// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_context_menu_helper.h"

#import "base/metrics/histogram_functions.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_observer_bridge.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/tab_context_menu_delegate.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_menu_actions_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GridContextMenuHelper () <BrowserObserving,
                                     GridContextMenuProvider> {
  // Observe BrowserObserver to prevent any access to Browser before its
  // destroyed.
  std::unique_ptr<BrowserObserverBridge> _browserObserver;
}

@property(nonatomic, assign) Browser* browser;
@property(nonatomic, weak) id<TabContextMenuDelegate> contextMenuDelegate;
@property(nonatomic, weak) id<GridMenuActionsDataSource> actionsDataSource;
@property(nonatomic, assign) BOOL incognito;
@end

@implementation GridContextMenuHelper

#pragma mark - GridContextMenuProvider

- (instancetype)initWithBrowser:(Browser*)browser
              actionsDataSource:(id<GridMenuActionsDataSource>)actionsDataSource
         tabContextMenuDelegate:
             (id<TabContextMenuDelegate>)tabContextMenuDelegate {
  self = [super init];
  if (self) {
    _browser = browser;
    _browserObserver = std::make_unique<BrowserObserverBridge>(_browser, self);
    _contextMenuDelegate = tabContextMenuDelegate;
    _actionsDataSource = actionsDataSource;
    _incognito = _browser->GetBrowserState()->IsOffTheRecord();
  }
  return self;
}

- (void)dealloc {
  if (self.browser) {
    _browserObserver.reset();
    self.browser = nullptr;
  }
}

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForGridCell:(GridCell*)gridCell
                           menuScenario:(MenuScenarioHistogram)scenario {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        GridContextMenuHelper* strongSelf = weakSelf;
        if (!strongSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        NSArray<UIMenuElement*>* menuElements =
            [strongSelf menuElementsForGridCell:gridCell menuScenario:scenario];
        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (NSArray<UIMenuElement*>*)menuElementsForGridCell:(GridCell*)gridCell
                                       menuScenario:
                                           (MenuScenarioHistogram)scenario {
  // Record that this context menu was shown to the user.
  RecordMenuShown(scenario);

  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:scenario];

  GridItem* item = [self.actionsDataSource
      gridItemForCellIdentifier:gridCell.itemIdentifier];
  if (!item) {
    return @[];
  }

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  if (!IsURLNewTabPage(item.URL)) {
    if ([self.contextMenuDelegate
            respondsToSelector:@selector(shareURL:title:scenario:fromView:)]) {
      [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                      [self.contextMenuDelegate
                          shareURL:item.URL
                             title:item.title
                          scenario:ActivityScenario::TabGridItem
                          fromView:gridCell];
                    }]];
    }

    if (IsPinnedTabsEnabled()) {
      if ([self.contextMenuDelegate
              respondsToSelector:@selector(pinTabWithIdentifier:incognito:)]) {
        [menuElements addObject:[actionFactory actionToPinTabWithBlock:^{
                        [self.contextMenuDelegate
                            pinTabWithIdentifier:gridCell.itemIdentifier
                                       incognito:self.incognito];
                      }]];
      }
    }

    if (item.URL.SchemeIsHTTPOrHTTPS() &&
        [self.contextMenuDelegate
            respondsToSelector:@selector(addToReadingListURL:title:)]) {
      [menuElements
          addObject:[actionFactory actionToAddToReadingListWithBlock:^{
            [self.contextMenuDelegate addToReadingListURL:item.URL
                                                    title:item.title];
          }]];
    }

    UIAction* bookmarkAction;
    bool currentlyBookmarked =
        [self.actionsDataSource isGridItemBookmarked:item];
    if (currentlyBookmarked) {
      if ([self.contextMenuDelegate
              respondsToSelector:@selector(editBookmarkWithURL:)]) {
        bookmarkAction = [actionFactory actionToEditBookmarkWithBlock:^{
          [self.contextMenuDelegate editBookmarkWithURL:item.URL];
        }];
      }
    } else {
      if ([self.contextMenuDelegate
              respondsToSelector:@selector(bookmarkURL:title:)]) {
        bookmarkAction = [actionFactory actionToBookmarkWithBlock:^{
          [self.contextMenuDelegate bookmarkURL:item.URL title:item.title];
        }];
      }
    }
    // Bookmarking can be disabled from prefs (from an enterprise policy),
    // if that's the case grey out the option in the menu.
    if (self.browser) {
      BOOL isEditBookmarksEnabled =
          self.browser->GetBrowserState()->GetPrefs()->GetBoolean(
              bookmarks::prefs::kEditBookmarksEnabled);
      if (!isEditBookmarksEnabled && bookmarkAction)
        bookmarkAction.attributes = UIMenuElementAttributesDisabled;
      if (bookmarkAction)
        [menuElements addObject:bookmarkAction];
    }
  }

  // Thumb strip and search results menus don't support tab selection.
  BOOL scenarioDisablesSelection =
      scenario == MenuScenarioHistogram::kTabGridSearchResult ||
      scenario == MenuScenarioHistogram::kThumbStrip;
  if (!scenarioDisablesSelection &&
      [self.contextMenuDelegate respondsToSelector:@selector(selectTabs)]) {
    [menuElements addObject:[actionFactory actionToSelectTabsWithBlock:^{
                    [self.contextMenuDelegate selectTabs];
                  }]];
  }

  if ([self.contextMenuDelegate
          respondsToSelector:@selector(closeTabWithIdentifier:incognito:)]) {
    [menuElements addObject:[actionFactory actionToCloseTabWithBlock:^{
                    [self.contextMenuDelegate
                        closeTabWithIdentifier:gridCell.itemIdentifier
                                     incognito:self.incognito];
                  }]];
  }
  return menuElements;
}

#pragma mark - BrowserObserving

- (void)browserDestroyed:(Browser*)browser {
  DCHECK_EQ(browser, self.browser);
  _browserObserver.reset();
  self.browser = nullptr;
}

@end
