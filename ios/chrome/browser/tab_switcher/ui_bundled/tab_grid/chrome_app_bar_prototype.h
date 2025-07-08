// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_CHROME_APP_BAR_PROTOTYPE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_CHROME_APP_BAR_PROTOTYPE_H_

#import <UIKit/UIKit.h>

@interface ChromeAppBarPrototype : UIView

// The different buttons of the app bar.
@property(nonatomic, strong) UIButton* askGeminiButton;
@property(nonatomic, strong) UIButton* openNewTabButton;
@property(nonatomic, strong) UIButton* tabGridButton;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_CHROME_APP_BAR_PROTOTYPE_H_
