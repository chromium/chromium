// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_UTILS_H_

#import <UIKit/UIKit.h>

// Helper class for the tab strip.
@interface TabStripHelper : NSObject

// The background color to be used.
@property(class, nonatomic, readonly) UIColor* backgroundColor;

// The color of the symbol of the new tab button.
@property(class, nonatomic, readonly) UIColor* newTabButtonSymbolColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_UTILS_H_
