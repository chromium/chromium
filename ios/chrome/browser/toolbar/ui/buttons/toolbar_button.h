// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"

using ToolbarButtonImageLoader = UIImage* (^)(void);

// Button displayed in the toolbar.
@interface ToolbarButton : UIButton

// The visibility mask for this button.
@property(nonatomic, assign) ToolbarButtonVisibility visibilityMask;

// When true the button is hidden, no matter the visibility mask. Default NO.
@property(nonatomic, assign) BOOL forceHidden;

// The `imageLoader` for this button.
- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTON_H_
