// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_BUTTON_CONFIGURATION_UTIL_H_
#define IOS_CHROME_COMMON_BUTTON_CONFIGURATION_UTIL_H_

#import <UIKit/UIKit.h>

// Sets ContentEdgeInsets for the button.
void SetContentEdgeInsets(UIButton* button, UIEdgeInsets insets);

// Sets ImageEdgeInsets for the button.
void SetImageEdgeInsets(UIButton* button, UIEdgeInsets insets);

// Sets TitleEdgeInsets for the button.
void SetTitleEdgeInsets(UIButton* button, UIEdgeInsets insets);

// Sets AdjustsImageWhenHighlighted for the button.
void SetAdjustsImageWhenHighlighted(UIButton* button, bool isHighlighted);

#endif  // IOS_CHROME_COMMON_BUTTON_CONFIGURATION_UTIL_H_
