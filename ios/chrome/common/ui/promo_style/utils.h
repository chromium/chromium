// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_PROMO_STYLE_UTILS_H_
#define IOS_CHROME_COMMON_UI_PROMO_STYLE_UTILS_H_

#import <UIKit/UIKit.h>

// Determines which font text style to use depending on the device size, the
// size class and if dynamic type is enabled.
UIFontTextStyle GetTitleLabelFontTextStyle(UIViewController* view_controller);

// Returns the title font for the FRE, based on `text_style`.
UIFont* GetFRETitleFont(UIFontTextStyle text_style);

// Creates, adds to `view` and returns width layout guide for promo style view
// controller (or similar view controllers). The width is 80% for large screens,
// and at max `kPromoStyleDefaultMargin` margin.
UILayoutGuide* AddPromoStyleWidthLayoutGuide(UIView* view);

#endif  // IOS_CHROME_COMMON_UI_PROMO_STYLE_UTILS_H_
