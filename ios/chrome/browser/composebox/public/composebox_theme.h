// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_THEME_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_THEME_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/public/composebox_input_plate_position.h"

// Contains UI configuration hints for the composebox components.
@interface ComposeboxTheme : NSObject

// The preferred position of the input plate.
@property(nonatomic, readonly) ComposeboxInputPlatePosition inputPlatePosition;

// Convenience check for input plate position top.
@property(nonatomic, readonly) BOOL isTopInputPlate;

// The background color for the composebox;
@property(nonatomic, readonly) UIColor* composeboxBackgroundColor;

// The background color for the input plate.
@property(nonatomic, readonly) UIColor* inputPlateBackgroundColor;

// The background color for the input item.
@property(nonatomic, readonly) UIColor* inputItemBackgroundColor;

// The color of the text in AIM button.
- (UIColor*)aimButtonTextColorWithAIMEnabled:(BOOL)AIMEnabled;

// The background color of the AIM button when enabled.
- (UIColor*)aimButtonBackgroundColorWithAIMEnabled:(BOOL)AIMEnabled;

// Creates a newc instance with the given configuration
- (instancetype)initWithInputPlatePosition:
    (ComposeboxInputPlatePosition)position;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_THEME_H_
