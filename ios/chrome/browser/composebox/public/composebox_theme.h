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

// Whether the theme is associated with an incognito session.
@property(nonatomic, readonly) BOOL incognito;

// Convenience check for input plate position top.
@property(nonatomic, readonly) BOOL isTopInputPlate;

// The background color for the composebox;
@property(nonatomic, readonly) UIColor* composeboxBackgroundColor;

// The background color for the input plate.
@property(nonatomic, readonly) UIColor* inputPlateBackgroundColor;

// The background color for the input item.
@property(nonatomic, readonly) UIColor* inputItemBackgroundColor;

// The background color for the close button.
@property(nonatomic, readonly) UIColor* closeButtonBackgroundColor;

// The color of the text in AIM button.
- (UIColor*)aimButtonTextColorWithAIMEnabled:(BOOL)AIMEnabled;

// The background color of the AIM button when enabled.
- (UIColor*)aimButtonBackgroundColorWithAIMEnabled:(BOOL)AIMEnabled;

// The color of the text in the image generation button.
- (UIColor*)imageGenerationButtonTextColor;

// The background color of the image generation button.
- (UIColor*)imageGenerationButtonBackgroundColor;

// The foreground color for the send button.
- (UIColor*)sendButtonForegroundColorHighlighted:(BOOL)highlighted;

// The background color for the send button.
- (UIColor*)sendButtonBackgroundColorHighlighted:(BOOL)highlighted;

// The color of the PDF symbol.
- (UIColor*)pdfSymbolColor;

// Creates a newc instance with the given configuration
- (instancetype)initWithInputPlatePosition:
                    (ComposeboxInputPlatePosition)position
                                 incognito:(BOOL)incognito;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_THEME_H_
