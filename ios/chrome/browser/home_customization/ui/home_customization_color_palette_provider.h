// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_PROVIDER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_PROVIDER_H_

#import <UIKit/UIKit.h>

#import "ui/color/color_provider_key.h"

@class NewTabPageColorPalette;

// A protocol for providing a color palette used in Home customization.
@protocol HomeCustomizationColorPaletteProvider

// Provides the color palette object from a seed color and a variant.
- (NewTabPageColorPalette*)
    provideColorPaletteFromSeedColor:(UIColor*)seedColor
                        colorVariant:
                            (ui::ColorProviderKey::SchemeVariant)colorVariant;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_PROVIDER_H_
