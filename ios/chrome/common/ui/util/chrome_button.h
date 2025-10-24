// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_CHROME_BUTTON_H_
#define IOS_CHROME_COMMON_UI_UTIL_CHROME_BUTTON_H_

#import <UIKit/UIKit.h>

// Styles for the buttons.
typedef NS_ENUM(NSInteger, ChromeButtonStyle) {
  ChromeButtonStylePrimary,
  ChromeButtonStylePrimaryDestructive,
  ChromeButtonStyleSecondary,
  ChromeButtonStyleTertiary,
};

// Image for the primary buttons.
typedef NS_ENUM(NSInteger, PrimaryButtonImage) {
  // Default image.
  PrimaryButtonImageNone,
  // Loading state, a spinner is shown.
  PrimaryButtonImageSpinner,
  // Confirmation state, a checkmark is shown.
  PrimaryButtonImageCheckmark,
};

// A chrome implementation of a UIButton.
@interface ChromeButton : UIButton

// The button's style. Setting this property will update the button's
// appearance to match the new style.
@property(nonatomic, assign) ChromeButtonStyle style;

// The button's title.
@property(nonatomic, copy) NSString* title;

// The button's font.
@property(nonatomic, copy) UIFont* font;

// The button's image. This property can only be set for primary style buttons.
@property(nonatomic, assign) PrimaryButtonImage primaryButtonImage;

// Whether the button has a tuned-down state. Default is NO. Takes precedence
// over "enabled" state.
@property(nonatomic, assign) BOOL tunedDownStyle;

// Designated init.
- (instancetype)initWithStyle:(ChromeButtonStyle)style
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithConfiguration:(UIButtonConfiguration*)configuration
                        primaryAction:(UIAction*)primaryAction NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_COMMON_UI_UTIL_CHROME_BUTTON_H_
