// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_

#import <UIKit/UIKit.h>

// This import is against the style guide, but it is here to facilitate the use
// of the util as it is necessary each time the util is used.
#import "ios/chrome/common/ui/util/chrome_button.h"

extern const UIControlState UIControlStateTunedDown;

extern const CGFloat kButtonVerticalInsets;
extern const CGFloat kPrimaryButtonCornerRadius;

// Updates `button` to match a primary action style.
void UpdateButtonToMatchPrimaryAction(ChromeButton* button);

// Updates `button` to match a primary destruction action style.
void UpdateButtonToMatchPrimaryDestructiveAction(ChromeButton* button);

// Updates `button` to match a secondary action style.
void UpdateButtonToMatchSecondaryAction(ChromeButton* button);

// Updates `button` to match a tertiary action style.
void UpdateButtonToMatchTertiaryAction(ChromeButton* button);

// Returns primary action button.
ChromeButton* PrimaryActionButton();

// Returns primary destructive action button.
ChromeButton* PrimaryDestructiveActionButton();

// Returns secondary action button.
ChromeButton* SecondaryActionButton();

// Returns tertiary action button.
ChromeButton* TertiaryActionButton();

// Sets the title of `button` through `button.configuration`.
void SetConfigurationTitle(UIButton* button, NSString* newString);

// Sets the font of `button` through `button.configuration`.
void SetConfigurationFont(UIButton* button, UIFont* font);

// Sets the image of `button` through `button.configuration`.
void SetConfigurationImage(ChromeButton* button, UIImage* image);

#endif  // IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_
