// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_FEATURES_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_FEATURES_UTILS_H_

#import <Foundation/Foundation.h>

// An Objective-C wrapper around C++ methods.
@interface TabStripFeaturesUtils : NSObject

// Helper function to check if the position of the "new tab button" on the
// modern tab strip is dynamic.
@property(class, nonatomic, readonly) BOOL isModernTabStripNewTabButtonDynamic;

// Helper function to check if tab groups appear in the tab strip.
@property(class, nonatomic, readonly) BOOL isModernTabStripWithTabGroups;

// Whether the new tab button should be bigger and closer to the tab strip.
@property(class, nonatomic, readonly) BOOL hasCloserNTB;

// Whether the tab strip should have a darker background.
@property(class, nonatomic, readonly) BOOL hasDarkerBackground;

// Whether the tab strip should have a darker background for V3.
@property(class, nonatomic, readonly) BOOL hasDarkerBackgroundV3;

// Whether the new tab button should have its background removed.
@property(class, nonatomic, readonly) BOOL hasNoNTBBackground;

// Whether the tab strip should have a black background.
@property(class, nonatomic, readonly) BOOL hasBlackBackground;

// Whether the NTB button should be bigger.
@property(class, nonatomic, readonly) BOOL hasBiggerNTB;

// Whether the close button of the non-selected tabs should always be visible.
@property(class, nonatomic, readonly) BOOL hasCloseButtonsVisible;

// Whether the non-selected tabs should have a higher contrast.
@property(class, nonatomic, readonly) BOOL hasHighContrastInactiveTabs;

// Whether the New Tab Button should have high contrast.
@property(class, nonatomic, readonly) BOOL hasHighContrastNTB;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_FEATURES_UTILS_H_
