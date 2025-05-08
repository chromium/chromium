// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUTOMIZATION_COLOR_PALETTE_CELL_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUTOMIZATION_COLOR_PALETTE_CELL_H_

#import <UIKit/UIKit.h>

// Configuration used to populate the color palette cell.
@class HomeCustomizationColorPaletteConfiguration;

// A collection view cell that displays a single background color palette option
// for the background customization.
@interface HomeCustomizationColorPaletteCell : UICollectionViewCell

// The configuration model that specifies the colors shown in the cell.
@property(nonatomic, strong)
    HomeCustomizationColorPaletteConfiguration* configuration;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUTOMIZATION_COLOR_PALETTE_CELL_H_
