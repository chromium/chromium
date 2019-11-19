// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_style.h"

// Toolbar configuration object giving access to styling elements.
@interface ToolbarConfiguration : NSObject

// Init the toolbar configuration with the desired |style|.
- (instancetype)initWithStyle:(ToolbarStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Style of this configuration.
@property(nonatomic, assign) ToolbarStyle style;

// Background color of the NTP. Used to do as if the toolbar was transparent and
// the NTP is visible behind it.
@property(nonatomic, readonly) UIColor* NTPBackgroundColor;

// Background color of the toolbar.
@property(nonatomic, readonly) UIColor* backgroundColor;

// Tint color of the buttons.
@property(nonatomic, readonly) UIColor* buttonsTintColor;

// Tint color of the buttons in the highlighted state. This is only to be used
// if the button has a custom style.
@property(nonatomic, readonly) UIColor* buttonsTintColorHighlighted;

// Color for the spotlight view's background.
@property(nonatomic, readonly) UIColor* buttonsSpotlightColor;

// Color for the spotlight view's background when the toolbar is dimmed.
@property(nonatomic, readonly) UIColor* dimmedButtonsSpotlightColor;

// Returns the background color of the location bar, with a |visibilityFactor|.
// The |visibilityFactor| is here to alter the alpha value of the background
// color. Even with a |visibilityFactor| of 1, the final color could is
// translucent.
- (UIColor*)locationBarBackgroundColorWithVisibility:(CGFloat)visibilityFactor;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_CONFIGURATION_H_
