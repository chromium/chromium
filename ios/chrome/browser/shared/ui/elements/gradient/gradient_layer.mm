// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_layer.h"

namespace {

// Returns step-based gradient colors from `start_color` to `end_color`
// using the provided `ease_function`.
NSArray* MultiStepGradientColors(UIColor* start_color,
                                 UIColor* end_color,
                                 float (*ease_function)(float)) {
  NSMutableArray* colors = [NSMutableArray array];
  const int kSteps = 16;

  CGFloat red_start, green_start, blue_start, alpha_start;
  if (![start_color getRed:&red_start
                     green:&green_start
                      blue:&blue_start
                     alpha:&alpha_start]) {
    // Handle case where color isn't compatible (e.g. pattern image), fallback
    // to white.
    red_start = green_start = blue_start = alpha_start = 1.0;
  }

  CGFloat red_end, green_end, blue_end, alpha_end;
  if (![end_color getRed:&red_end
                   green:&green_end
                    blue:&blue_end
                   alpha:&alpha_end]) {
    red_end = green_end = blue_end = alpha_end = 1.0;
  }

  for (int i = 0; i <= kSteps; i++) {
    float progress = (float)i / (float)kSteps;

    float eased_progress = ease_function(progress);

    CGFloat red = red_start + (red_end - red_start) * eased_progress;
    CGFloat green = green_start + (green_end - green_start) * eased_progress;
    CGFloat blue = blue_start + (blue_end - blue_start) * eased_progress;
    CGFloat alpha = alpha_start + (alpha_end - alpha_start) * eased_progress;

    UIColor* color = [UIColor colorWithRed:red
                                     green:green
                                      blue:blue
                                     alpha:alpha];
    [colors addObject:(id)color.CGColor];
  }

  return colors;
}

}  // namespace

@implementation GradientLayer

- (void)setStartColor:(UIColor*)startColor endColor:(UIColor*)endColor {
  switch (self.gradientType) {
    case GradientLayerType::kLinear:
      self.colors = @[
        (id)startColor.CGColor,
        (id)endColor.CGColor,
      ];
      break;
    case GradientLayerType::kEaseInOut:
      self.colors =
          MultiStepGradientColors(startColor, endColor, [](float progress) {
            return -(cosf(M_PI * progress) - 1.0f) / 2.0f;
          });
      break;
    case GradientLayerType::kEaseInThenLinear:
      self.colors =
          MultiStepGradientColors(startColor, endColor, [](float progress) {
            if (progress <= 0.5f) {
              return (1.0f - cosf(progress * M_PI)) / 2.0f;
            } else {
              return progress;
            }
          });
      break;
  }
}

@end
