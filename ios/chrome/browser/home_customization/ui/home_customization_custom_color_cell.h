// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_CUSTOM_COLOR_CELL_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_CUSTOM_COLOR_CELL_H_

#import <UIKit/UIKit.h>

// A collection view cell that displays a single background color option picked
// from a color picker for the background customization.
@interface HomeCustomizationCustomColorCell : UICollectionViewCell

// The color used to display the cell's colors.
@property(nonatomic, strong) UIColor* color;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_CUSTOM_COLOR_CELL_H_
