// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_UTILS_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_UTILS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/web/public/web_state_id.h"

class Browser;
class BrowserList;
@class GridItemIdentifier;
@class TabItem;
@class TabSwitcherItem;
class WebStateList;

// Enum for the various layouts for the empty thumbnail of a tab grid view.
typedef NS_ENUM(NSInteger, EmptyThumbnailLayoutType) {
  EmptyThumbnailLayoutTypeLandscape = 1,
  EmptyThumbnailLayoutTypePortrait,
  EmptyThumbnailLayoutTypeCenteredPortrait,
  EmptyThumbnailLayoutTypeLandscapeLeading,
};

// Returns the aspect ratio (height / width) of an item based on the container
// `size`.
CGFloat TabGridItemAspectRatio(CGSize size);

// Returns the number of columns based on based on the container `size` and
// `content_size_category`.
NSInteger TabGridColumnsCount(CGSize size,
                              UIContentSizeCategory content_size_category);

// Returns the TabItem object representing the tab with the given `criteria`.
// Returns `nil` if the tab is not found.
TabItem* GetTabItem(WebStateList* web_state_list,
                    WebStateSearchCriteria criteria);

// Returns whether `items` has items (of type group or tab) with the same
// identifier.
bool HasDuplicateGroupsAndTabsIdentifiers(NSArray<GridItemIdentifier*>* items);

// Returns whether `items` has items with the same identifier.
bool HasDuplicateIdentifiers(NSArray<TabSwitcherItem*>* items);

// Returns the Browser with a tab matching `criteria` in its WebStateList.
// Returns `nullptr` if not found.
Browser* GetBrowserForTabWithCriteria(BrowserList* browser_list,
                                      WebStateSearchCriteria criteria,
                                      bool is_otr_tab);

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_UTILS_H_
