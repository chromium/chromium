// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_NTP_THEME_ACTIONABLE_TIP_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_NTP_THEME_ACTIONABLE_TIP_VIEW_H_

#import <UIKit/UIKit.h>

@class NTPThemeActionableTipConfig;

// View displaying the 5 circular swatches for the actionable NTP Theme tip.
@interface NTPThemeActionableTipView : UIView

// Initializes the view with `config`.
- (instancetype)initWithConfig:(NTPThemeActionableTipConfig*)config
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_NTP_THEME_ACTIONABLE_TIP_VIEW_H_
