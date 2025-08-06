// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_CHROME_BUTTON_H_
#define IOS_CHROME_COMMON_UI_UTIL_CHROME_BUTTON_H_

#import <UIKit/UIKit.h>

// A chrome implementation of a UIButton.
@interface ChromeButton : UIButton

// Whether the button has a tuned-down state. Default is NO. Takes precedence
// over "enabled" state.
@property(nonatomic, assign) BOOL tunedDownStyle;

@end

#endif  // IOS_CHROME_COMMON_UI_UTIL_CHROME_BUTTON_H_
