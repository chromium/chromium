// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_UTIL_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_UTIL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_configuration.h"

// Creates and returns a color palette configuration from a seed color.
HomeCustomizationColorPaletteConfiguration*
CreateColorPaletteConfigurationFromSeedColor(UIColor* seedColor);

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLOR_PALETTE_UTIL_H_
