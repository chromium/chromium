// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_TRACK_BUTTON_UTIL_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_TRACK_BUTTON_UTIL_H_

#import <UIKit/UIKit.h>

namespace price_notifications {

struct WidthConstraintValues {
  // The maximum width the Track Button can occupy on the UI.
  size_t max_width;
  // The ideal width the Track Button will occupy on the UI.
  size_t target_width;
};

// Calculates the track button's appropriate horizontal padding relative to the
// size of the Track Button's parent container and the space the button's text
// occupies.
size_t CalculateTrackButtonHorizontalPadding(double parent_cell_width,
                                             double button_text_width);

// Calculates the target and maximum width the Track Button can occupy relative
// to the size of the Track Button's parent container and the width of its
// content.
WidthConstraintValues CalculateTrackButtonWidthConstraints(
    double parent_cell_width,
    double button_text_width,
    size_t horizontal_padding);

}  // namespace price_notifications

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_TRACK_BUTTON_UTIL_H_
