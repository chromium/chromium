// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/util/chrome_button.h"

extern const CGFloat kButtonVerticalInsets;
extern const CGFloat kPrimaryButtonCornerRadius;

// Sets the title of `button` through `button.configuration`.
void SetConfigurationTitle(UIButton* button, NSString* newString);

// Sets the font of `button` through `button.configuration`.
void SetConfigurationFont(UIButton* button, UIFont* font);

// DEPRECATED: Most of the functionality in this file has been moved to
// the ChromeButton class itself. Please use its properties instead.
void UpdateButtonToMatchPrimaryAction(ChromeButton* button);
void UpdateButtonToMatchPrimaryDestructiveAction(ChromeButton* button);
void UpdateButtonToMatchSecondaryAction(ChromeButton* button);
void UpdateButtonToMatchTertiaryAction(ChromeButton* button);
ChromeButton* PrimaryActionButton();
ChromeButton* PrimaryDestructiveActionButton();
ChromeButton* SecondaryActionButton();
ChromeButton* TertiaryActionButton();

#endif  // IOS_CHROME_COMMON_UI_UTIL_BUTTON_UTIL_H_
