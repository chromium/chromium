// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_NAVIGATION_BUTTONS_CONTAINER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_NAVIGATION_BUTTONS_CONTAINER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_element_with_background.h"

@class ToolbarButton;

// Container for the back and forward buttons.
@interface ToolbarNavigationButtonsContainer
    : UIView <ToolbarElementWithBackground>

// Init with the back and forward buttons.
- (instancetype)initWithBackButton:(ToolbarButton*)backButton
                     forwardButton:(ToolbarButton*)forwardButton
                         incognito:(BOOL)incognito NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_NAVIGATION_BUTTONS_CONTAINER_H_
