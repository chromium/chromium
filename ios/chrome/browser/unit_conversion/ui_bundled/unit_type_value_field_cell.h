// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_TYPE_VALUE_FIELD_CELL_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_TYPE_VALUE_FIELD_CELL_H_

#import <UIKit/UIKit.h>

// The cell where the unit value is displayed and where it can be modified.
@interface UnitTypeValueFieldCell : UITableViewCell

// The text field for the unit value.
@property(nonatomic, strong) UITextField* unitValueTextField;

@end

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_TYPE_VALUE_FIELD_CELL_H_
