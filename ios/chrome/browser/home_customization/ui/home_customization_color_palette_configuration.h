// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// A model object that defines a background color palette, including
// light, medium, and dark variants derived from a seed color.
@interface HomeCustomizationColorPaletteConfiguration : NSObject

// A lighter tone variant of the seed color.
@property(nonatomic, strong) UIColor* lightColor;

// A medium tone variant of the seed color.
@property(nonatomic, strong) UIColor* mediumColor;

// A darker tone variant of the seed color.
@property(nonatomic, strong) UIColor* darkColor;

// The original seed color used to generate the palette.
@property(nonatomic, strong) UIColor* seedColor;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_CONFIGURATION_H_
