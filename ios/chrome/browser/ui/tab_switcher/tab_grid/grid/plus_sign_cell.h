// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_PLUS_SIGN_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_PLUS_SIGN_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_theme.h"

// A square-ish cell in a grid that contains a plus sign.
// TODO(crbug.com/1137983): Add Accessibility label to the plus sign cell.
// TODO(crbug.com/1137986): Add eg2 tests and unit tests.
@interface PlusSignCell : UICollectionViewCell
// The look of the cell.
@property(nonatomic, assign) GridTheme theme;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_PLUS_SIGN_CELL_H_
