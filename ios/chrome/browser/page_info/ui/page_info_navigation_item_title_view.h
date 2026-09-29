// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_UI_PAGE_INFO_NAVIGATION_ITEM_TITLE_VIEW_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_UI_PAGE_INFO_NAVIGATION_ITEM_TITLE_VIEW_H_

#import <UIKit/UIKit.h>

// Returns a navigation item title view that displays `title` and `siteURL`.
// `siteURL` uses `NSLineBreakByTruncatingHead` so that the registrable domain
// remains visible when truncated.
UIView* CreatePageInfoNavigationItemTitleView(NSString* title,
                                              NSString* siteURL);

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_UI_PAGE_INFO_NAVIGATION_ITEM_TITLE_VIEW_H_
