// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_TOOLS_MENU_BUTTON_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_TOOLS_MENU_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"

// A custom button for the NTP tools menu.
@interface NTPToolsMenuButton : ExtendedTouchTargetButton

// Controls the visibility of the blue dot.
@property(nonatomic, assign, getter=hasBlueDot) BOOL blueDot;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_TOOLS_MENU_BUTTON_H_
