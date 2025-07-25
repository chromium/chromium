// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_CHROME_APP_BAR_PROTOTYPE_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_CHROME_APP_BAR_PROTOTYPE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

class Browser;
@class DiamondGridButton;

@interface ChromeAppBarPrototype : UIView

// The different buttons of the app bar.
@property(nonatomic, strong) UIButton* askGeminiButton;
@property(nonatomic, strong) UIButton* openNewTabButton;
@property(nonatomic, strong) DiamondGridButton* tabGridButton;

@property(nonatomic, assign) Browser* regularBrowser;
@property(nonatomic, assign) Browser* incognitoBrowser;
@property(nonatomic, assign) TabGridPage currentPage;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_CHROME_APP_BAR_PROTOTYPE_H_
