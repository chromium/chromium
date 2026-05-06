// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_LIST_CELL_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_LIST_CELL_H_

#import <UIKit/UIKit.h>

// A custom list cell that fixes a recursive layout loop crash in
// UICollectionView by rounding up the preferred height to the nearest point.
@interface ComposeboxMenuListCell : UICollectionViewListCell
@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_LIST_CELL_H_
