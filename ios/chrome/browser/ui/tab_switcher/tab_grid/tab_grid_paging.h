// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGING_H_

// Page enumerates the kinds of grouped tabs.
typedef NS_ENUM(NSUInteger, TabGridPage) {
  TabGridPageIncognitoTabs = 0,
  TabGridPageRegularTabs = 1,
  TabGridPageRemoteTabs = 2,
};

// Page enumerates the modes of the tab grid.
typedef NS_ENUM(NSUInteger, TabGridMode) {
  TabGridModeNormal = 0,
  TabGridModeSelection = 1,
  TabGridModeSearch = 2,
};

// An object implementing this protocol can change the active "page" or the mode
// of the tab grid.
@protocol TabGridPaging <NSObject>
// Active page of the tab grid. The active page is the page that
// contains the most recent active tab.
@property(nonatomic, assign) TabGridPage activePage;
@property(nonatomic, assign) TabGridMode tabGridMode;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGING_H_
