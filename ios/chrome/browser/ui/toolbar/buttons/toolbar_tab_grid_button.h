// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_TAB_GRID_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_TAB_GRID_BUTTON_H_

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"

enum class ToolbarTabGridButtonStyle;

using ToolbarTabGridButtonImageLoader =
    UIImage* (^)(ToolbarTabGridButtonStyle style);

// ToolbarButton for displaying the number of tab.
@interface ToolbarTabGridButton : ToolbarButton

// Sets the number of tabs displayed by this button to `tabCount`. If `tabCount`
// is more than 99, it shows a smiley instead. But the value stored in tabCount
// can be bigger than 100.
@property(nonatomic, assign) int tabCount;

// The current style for the button.
@property(nonatomic, assign) ToolbarTabGridButtonStyle tabGridButtonStyle;

// Initialized the button with an image loader that can return a different image
// based on the tab grid button style.
- (instancetype)initWithStyledImageLoader:
    (ToolbarTabGridButtonImageLoader)styledImageLoader
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader
    NS_UNAVAILABLE;
- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader
          IPHHighlightedImageLoader:
              (ToolbarButtonImageLoader)IPHHighlightedImageLoader
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_TAB_GRID_BUTTON_H_
