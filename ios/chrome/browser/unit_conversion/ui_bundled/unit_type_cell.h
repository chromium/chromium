// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_TYPE_CELL_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_TYPE_CELL_H_

#import <UIKit/UIKit.h>

// The cell where the unit (meters, pounds, etc) is displayed and the unit menu
// is triggered when the cell is tapped.
@interface UnitTypeCell : UITableViewCell

// The button to display the units UIMenu
@property(nonatomic, strong) UIButton* unitMenuButton;

@end

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_TYPE_CELL_H_
