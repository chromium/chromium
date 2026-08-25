// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_CARD_BACKGROUND_VIEW_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_CARD_BACKGROUND_VIEW_H_

#import <UIKit/UIKit.h>

// A reusable background view for NTP cards (Magic Stack modules, Most Visited
// Tiles, Feed container) that dynamically renders wallpaper frosted blur or
// solid/tinted background color according to NTP traits.
@interface NTPCardBackgroundView : UIView

// Fades the view in.
- (void)fadeIn;

// Fades the view out.
- (void)fadeOut;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_CARD_BACKGROUND_VIEW_H_
