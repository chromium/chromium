// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/icons/custom_symbol.h"

#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/image_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Background image color alpha.
constexpr CGFloat kBackgroundColorAlpha = 0.1;

// Offset of the background image used to create the elevated effect.
constexpr CGFloat kBackgroundImageOffset = 1;

// Background blur radius.
const CGFloat kBackgroundBlurRadius = 3;

// Background image corner radius.
constexpr CGFloat kBackgroundCornerRadius = 7;

}  // namespace

UIView* ElevatedTableViewSymbolWithBackground(UIImage* symbol,
                                              UIColor* background_color) {
  UIView* symbol_image_view = [[UIView alloc] init];
  symbol_image_view.backgroundColor = background_color;
  symbol_image_view.layer.cornerRadius = kBackgroundCornerRadius;

  CGFloat center_point = kTableViewIconImageSize / 2;

  UIImage* blurred_image = BlurredImageWithImage(symbol, kBackgroundBlurRadius);
  UIView* background_image_view =
      [[UIImageView alloc] initWithImage:blurred_image];
  background_image_view.center =
      CGPointMake(center_point, center_point + kBackgroundImageOffset);
  background_image_view.tintColor =
      [UIColor.blackColor colorWithAlphaComponent:kBackgroundColorAlpha];
  [symbol_image_view addSubview:background_image_view];

  UIView* foreground_image_view = [[UIImageView alloc] initWithImage:symbol];
  foreground_image_view.center = CGPointMake(center_point, center_point);
  foreground_image_view.tintColor = UIColor.whiteColor;
  [symbol_image_view addSubview:foreground_image_view];
  return symbol_image_view;
}
