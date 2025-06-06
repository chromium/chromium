// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_PROVIDER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_PROVIDER_H_

#import <UIKit/UIKit.h>

@class HomeCustomizationColorPaletteConfiguration;

// A protocol for providing a color palette used in Home customization.
@protocol HomeCustomizationColorPaletteProvider

// Provides the color palette object from a seed color.
- (HomeCustomizationColorPaletteConfiguration*)provideColorPaletteFromSeedColor:
    (UIColor*)seedColor;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_PROVIDER_H_
