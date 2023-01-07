// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MENU_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MENU_PROVIDER_H_

#import <UIKit/UIKit.h>

@class TableViewURLItem;

// Protocol for instances that will provide menus to RecentTabs components.
@protocol RecentTabsMenuProvider

// Creates a context menu configuration instance for the given `item` and its
// associated `view`.
- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (TableViewURLItem*)item
                                                      fromView:(UIView*)view;

// Creates a context menu configuration instance for the header of the given
// `sectionIdentifier`.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForHeaderWithSectionIdentifier:
        (NSInteger)sectionIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MENU_PROVIDER_H_
