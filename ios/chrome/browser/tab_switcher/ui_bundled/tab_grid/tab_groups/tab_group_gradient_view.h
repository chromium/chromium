// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_GRADIENT_VIEW_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_GRADIENT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"

// A gradient background used in the TabGroupViewController and
// CreateTabGroupViewController.
@interface TabGroupGradientView : GradientView

// Returns a vertical gradient background of 3 colors.
- (instancetype)initWithColors:(NSArray*)colors;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_GRADIENT_VIEW_H_
