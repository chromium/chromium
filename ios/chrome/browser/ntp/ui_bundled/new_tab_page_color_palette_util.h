// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COLOR_PALETTE_UTIL_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COLOR_PALETTE_UTIL_H_

#import <UIKit/UIKit.h>

#import "ui/color/color_provider_key.h"

@class NewTabPageColorPalette;

// Creates and returns a color palette configuration from a seed color.
NewTabPageColorPalette* CreateColorPaletteFromSeedColor(
    UIColor* seed_color,
    ui::ColorProviderKey::SchemeVariant variant);

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COLOR_PALETTE_UTIL_H_
