// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_

#import <UIKit/UIKit.h>

extern const CGFloat kButtonVerticalInsets;
extern const CGFloat kPrimaryButtonCornerRadius;

// Returns primary action button with rounded corners.
UIButton* PrimaryActionButton(BOOL pointer_interaction_enabled);

// Sets the title of `button` through `button.configuration`.
void SetConfigurationTitle(UIButton* button, NSString* newString);

// Sets the font of `button` through `button.configuration`.
void SetConfigurationFont(UIButton* button, UIFont* font);

// Updates the button configuration if the button is enabled or disabled.
void UpdateButtonColorOnEnableDisable(UIButton* button);

#endif  // IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_
