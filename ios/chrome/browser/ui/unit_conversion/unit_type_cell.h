// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UNIT_CONVERSION_UNIT_TYPE_CELL_H_
#define IOS_CHROME_BROWSER_UI_UNIT_CONVERSION_UNIT_TYPE_CELL_H_

#import <UIKit/UIKit.h>

// The cell where the unit (meters, pounds, etc) and the unit menu button are
// displayed.
@interface UnitTypeCell : UITableViewCell

// The button to display the units UIMenu
@property(nonatomic, strong) UIButton* unitMenuButton;

// The label to display the unit.
@property(nonatomic, strong) UILabel* unitTypeLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_UNIT_CONVERSION_UNIT_TYPE_CELL_H_
