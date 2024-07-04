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
+ (BOOL)isModernTabStripNewTabButtonDynamic;

// Helper function to check if tab groups appear in the tab strip.
+ (BOOL)isModernTabStripWithTabGroups;

// Whether the new tab button should be bigger and closer to the tab strip.
+ (BOOL)isTabStripCloserNTBEnabled;

// Whether the tab strip should have a darker background.
+ (BOOL)isTabStripDarkerBackgroundEnabled;

// Whether the tab strip should have a darker background and a closer new tab
// button.
+ (BOOL)isTabStripCloserNTBDarkerBackgroundEnabled;

// Whether the new tab button should have its background removed.
+ (BOOL)isTabStripNTBNoBackgroundEnabled;

// Whether the tab strip should have a black background.
+ (BOOL)isTabStripBlackBackgroundEnabled;

// Whether the close button should have a bigger tap target.
+ (BOOL)isTabStripBiggerCloseTargetEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_FEATURES_UTILS_H_
