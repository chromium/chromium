// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tabs/tab_strip_context_menu_helper.h"

#import "base/metrics/histogram_functions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/main/browser_observer_bridge.h"
#import "ios/chrome/browser/ntp/new_tab_page_util.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_context_menu_delegate.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabStripContextMenuHelper () <BrowserObserving,
                                         TabStripContextMenuProvider> {
  // Observe BrowserObserver to prevent any access to Browser before its
  // destroyed.
  std::unique_ptr<BrowserObserverBridge> _browserObserver;
}

@property(nonatomic, assign) Browser* browser;
@property(nonatomic, weak) id<TabStripContextMenuDelegate> delegate;

@end

@implementation TabStripContextMenuHelper

#pragma mark - TabStripContextMenuProvider

- (instancetype)initWithBrowser:(Browser*)browser
    tabStripContextMenuDelegate:
        (id<TabStripContextMenuDelegate>)tabStripContextMenuDelegate {
  self = [super init];
  if (self) {
    _browser = browser;
    _browserObserver = std::make_unique<BrowserObserverBridge>(_browser, self);
    _delegate = tabStripContextMenuDelegate;
  }
  return self;
}

- (void)dealloc {
  _browserObserver.reset();
  _browser = nullptr;
}

#pragma mark - TabStripContextMenuProvider

- (UIMenu*)menuForWebStateIdentifier:(NSString*)identifier
                         pinnedState:(BOOL)pinnedState {
  NSArray<UIMenuElement*>* menuElements =
      [self menuElementsForWebstateIdentifier:identifier
                                  pinnedState:pinnedState];
  return [UIMenu menuWithTitle:@"" children:menuElements];
}

#pragma mark - Private

// Returns context menu actions for the given `webStateIndex` and `pinnedState`.
- (NSArray<UIMenuElement*>*)
    menuElementsForWebstateIdentifier:(NSString*)identifier
                          pinnedState:(BOOL)pinnedState {
  // Record that this context menu was shown to the user.
  MenuScenarioHistogram scenario = MenuScenarioHistogram::kTabStripEntry;
  RecordMenuShown(scenario);

  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:scenario];

  TabItem* item = [self tabItemForIdentifier:identifier];

  if (!item) {
    return @[];
  }

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  BOOL pinnedActionsAvailable =
      IsPinnedTabsEnabled() && !_browser->GetBrowserState()->IsOffTheRecord();
  if (pinnedActionsAvailable) {
    if (pinnedState) {
      [menuElements addObject:[actionFactory actionToUnpinTabWithBlock:^{
                      [self.delegate unpinTabWithIdentifier:identifier];
                    }]];
    } else {
      [menuElements addObject:[actionFactory actionToPinTabWithBlock:^{
                      [self.delegate pinTabWithIdentifier:identifier];
                    }]];
    }
  }

  BOOL addToReadingListActionAvailable =
      !IsURLNewTabPage(item.URL) && item.URL.SchemeIsHTTPOrHTTPS();
  if (addToReadingListActionAvailable) {
    [menuElements addObject:[actionFactory actionToAddToReadingListWithBlock:^{
                    [self.delegate addToReadingListURL:item.URL
                                                 title:item.title];
                  }]];
  }

  BOOL bookmarksActionsAvailable =
      !IsURLNewTabPage(item.URL) && item.URL.SchemeIsHTTPOrHTTPS();
  if (bookmarksActionsAvailable) {
    UIAction* bookmarkAction;
    const BOOL currentlyBookmarked = [self isTabItemBookmarked:item];
    if (currentlyBookmarked) {
      bookmarkAction = [actionFactory actionToEditBookmarkWithBlock:^{
        [self.delegate editBookmarkWithURL:item.URL];
      }];
    } else {
      bookmarkAction = [actionFactory actionToBookmarkWithBlock:^{
        [self.delegate bookmarkURL:item.URL title:item.title];
      }];
    }
    // Bookmarking can be disabled from prefs (from an enterprise policy),
    // if that's the case grey out the option in the menu.
    if (self.browser) {
      BOOL isEditBookmarksEnabled =
          self.browser->GetBrowserState()->GetPrefs()->GetBoolean(
              bookmarks::prefs::kEditBookmarksEnabled);
      if (!isEditBookmarksEnabled && bookmarkAction) {
        bookmarkAction.attributes = UIMenuElementAttributesDisabled;
      }
      if (bookmarkAction) {
        [menuElements addObject:bookmarkAction];
      }
    }
  }

  [menuElements addObject:[actionFactory actionToCloseTabWithBlock:^{
                  [self.delegate closeTabWithIdentifier:identifier];
                }]];
  return menuElements;
}

#pragma mark - BrowserObserving

- (void)browserDestroyed:(Browser*)browser {
  DCHECK_EQ(browser, self.browser);
  _browserObserver.reset();
  self.browser = nullptr;
}

#pragma mark - Private

// Returns `YES` if the tab `item` is already bookmarked.
- (BOOL)isTabItemBookmarked:(TabItem*)item {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          _browser->GetBrowserState());
  return item && bookmarkModel &&
         bookmarkModel->GetMostRecentlyAddedUserNodeForURL(item.URL);
}

// Returns the TabItem object representing the tab with `identifier`.
- (TabItem*)tabItemForIdentifier:(NSString*)identifier {
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(_browser->GetBrowserState());
  std::set<Browser*> browsers = _browser->GetBrowserState()->IsOffTheRecord()
                                    ? browserList->AllIncognitoBrowsers()
                                    : browserList->AllRegularBrowsers();
  for (Browser* browser : browsers) {
    WebStateList* webStateList = browser->GetWebStateList();
    TabItem* item = GetTabItem(
        webStateList, WebStateSearchCriteria{.identifier = identifier});
    if (item) {
      return item;
    }
  }
  return nil;
}

@end
