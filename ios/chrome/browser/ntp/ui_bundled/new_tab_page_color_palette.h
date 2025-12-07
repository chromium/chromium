// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COLOR_PALETTE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COLOR_PALETTE_H_

#import <UIKit/UIKit.h>

#import "ui/color/color_provider_key.h"

// A model object that defines a background color palette, including
// light, medium, and dark variants derived from a seed color.
@interface NewTabPageColorPalette : NSObject

// A lighter tone variant of the seed color.
@property(nonatomic, strong) UIColor* lightColor;

// A medium tone variant of the seed color.
@property(nonatomic, strong) UIColor* mediumColor;

// A darker tone variant of the seed color.
@property(nonatomic, strong) UIColor* darkColor;

// A general-purpose accent or highlight color used across the UI.
@property(nonatomic, strong) UIColor* tintColor;

// The primary color used for main UI elements.
@property(nonatomic, strong) UIColor* primaryColor;

// A secondary color used for supporting UI elements.
@property(nonatomic, strong) UIColor* secondaryColor;

// A color specifically used for background cells or cards.
@property(nonatomic, strong) UIColor* secondaryCellColor;

// A tertiary color used for subtle accents or less prominent components.
@property(nonatomic, strong) UIColor* tertiaryColor;

// The background color used for the omnibox.
@property(nonatomic, strong) UIColor* omniboxColor;

// The color used for icons within the omnibox.
@property(nonatomic, strong) UIColor* omniboxIconColor;

// The color used for dividers or separators between omnibox icons.
@property(nonatomic, strong) UIColor* omniboxIconDividerColor;

// The original seed color used to generate the palette.
@property(nonatomic, strong) UIColor* seedColor;

// The color used for the monograms.
@property(nonatomic, strong) UIColor* monogramColor;

// The color used for the header buttons.
@property(nonatomic, strong) UIColor* headerButtonColor;

// The color scheme variant that was used along with the seed color to generate
// the palette.
@property(nonatomic, assign) ui::ColorProviderKey::SchemeVariant variant;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COLOR_PALETTE_H_
