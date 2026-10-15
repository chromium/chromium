// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_NTP_THEME_ACTIONABLE_TIP_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_NTP_THEME_ACTIONABLE_TIP_CONFIG_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module.h"

@class NewTabPageColorPalette;
@protocol TipsModuleAudience;

// Config for the actionable NTP Theme tip module in the Magic Stack.
@interface NTPThemeActionableTipConfig : MagicStackModule

// The color palettes displayed as circular swatches in the module.
@property(nonatomic, copy) NSArray<NewTabPageColorPalette*>* colorPalettes;

// The thumbnail images for the preset background themes displayed as circular
// swatches in the module.
@property(nonatomic, copy) NSArray<UIImage*>* backgroundImages;

// The index of the currently selected color swatch.
@property(nonatomic, assign) NSInteger selectedIndex;

// The object that should handle user events.
@property(nonatomic, weak) id<TipsModuleAudience> audience;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_NTP_THEME_ACTIONABLE_TIP_CONFIG_H_
