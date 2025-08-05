// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_

#import <UIKit/UIKit.h>

extern const CGFloat kButtonVerticalInsets;
extern const CGFloat kPrimaryButtonCornerRadius;

// Updates `button` to match a primary action style.
void UpdateButtonToMatchPrimaryAction(UIButton* button);

// Updates `button` to match a secondary action style.
void UpdateButtonToMatchSecondaryAction(UIButton* button);

// Updates `button` to match a equal weight style.
void UpdateButtonToMatchEqualWeightAction(UIButton* button);

// Returns primary action button.
UIButton* PrimaryActionButton();

// Returns secondary action button.
UIButton* SecondaryActionButton();

// Returns equal weight button.
UIButton* EqualWeightButton();

// Sets the title of `button` through `button.configuration`.
void SetConfigurationTitle(UIButton* button, NSString* newString);

// Sets the font of `button` through `button.configuration`.
void SetConfigurationFont(UIButton* button, UIFont* font);

#endif  // IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_
