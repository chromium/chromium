// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FAKE_LOCATION_BAR_VIEW_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FAKE_LOCATION_BAR_VIEW_H_

#import <UIKit/UIKit.h>

@class NewTabPageColorPalette;

// A button that visually represents the fake omnibox on the NTP.
// It handles its own highlight state and background colors.
@interface FakeLocationBarView : UIButton

// Updates the background colors and highlight state based on progress.
- (void)updateColorsWithProgress:(CGFloat)progress
                    colorPalette:(NewTabPageColorPalette*)colorPalette;

// Updates the background theme (blur vs gradient) based on traits.
- (void)applyBackgroundTheme;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FAKE_LOCATION_BAR_VIEW_H_
