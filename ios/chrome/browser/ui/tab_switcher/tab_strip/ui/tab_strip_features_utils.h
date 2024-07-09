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
@property(class, nonatomic, readonly) BOOL isTabStripCloserNTBEnabled;

// Whether the tab strip should have a darker background.
@property(class, nonatomic, readonly) BOOL isTabStripDarkerBackgroundEnabled;

// Whether the tab strip should have a darker background and a closer new tab
// button.
@property(class, nonatomic, readonly)
    BOOL isTabStripCloserNTBDarkerBackgroundEnabled;

// Whether the new tab button should have its background removed.
@property(class, nonatomic, readonly) BOOL isTabStripNTBNoBackgroundEnabled;

// Whether the tab strip should have a black background.
@property(class, nonatomic, readonly) BOOL isTabStripBlackBackgroundEnabled;

// Whether any of the V2 experiments are enabled.
@property(class, nonatomic, readonly) BOOL isTabStripV2;

// Whether the close button should have a bigger tap target.
@property(class, nonatomic, readonly) BOOL isTabStripBiggerCloseTargetEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_FEATURES_UTILS_H_
