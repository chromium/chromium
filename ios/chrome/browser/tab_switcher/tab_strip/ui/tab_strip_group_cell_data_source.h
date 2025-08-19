// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

@protocol FacePileProviding;
@class TabStripItemIdentifier;

// Protocol for querying properties related to a `TabStripTabGroupCell`.
@protocol TabStripTabGroupCellDataSource

// Returns the facePile view associated with the item.
- (id<FacePileProviding>)facePileProviderForItem:
    (TabStripItemIdentifier*)itemIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_DATA_SOURCE_H_
