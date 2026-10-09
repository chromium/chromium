// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_zero_state_background_view.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"
#import "ios/chrome/browser/shared/ui/elements/gradient/multi_color_gradient_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Radial mask geometry for the zero-state halo effect.
// Anchoring at `{0.5, 0.25}` with a `{1.15, 1.0}` end point keeps the upper
// portion transparent and smoothly opens the mask lower in the container.
constexpr CGPoint kHaloRadialStartPoint = {0.5, 0.25};
constexpr CGPoint kHaloRadialEndPoint = {1.15, 1.0};
constexpr CGFloat kHaloMaxBlueAlpha = 0.65;

// Vertical gradient range spanning from `y = 0.20` down to `y = 0.85` so the
// blue halo blends gradually from transparent to full intensity.
constexpr CGPoint kHaloVerticalStartPoint = {0.5, 0.20};
constexpr CGPoint kHaloVerticalEndPoint = {0.5, 0.85};
}  // namespace

@implementation GeminiContainerZeroStateBackgroundView {
  GradientView* _gradientView;
  MultiColorGradientView* _maskView;
}

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.userInteractionEnabled = NO;
    [self setUpSubviews];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  _maskView.frame = self.bounds;
}

#pragma mark - Private

// Sets up the halo background gradient views.
- (void)setUpSubviews {
  self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  UIColor* haloBlueColor = [UIColor colorNamed:kBlue300Color];

  _gradientView = [[GradientView alloc]
      initWithStartColor:[haloBlueColor colorWithAlphaComponent:0.0]
                endColor:[haloBlueColor
                             colorWithAlphaComponent:kHaloMaxBlueAlpha]
              startPoint:kHaloVerticalStartPoint
                endPoint:kHaloVerticalEndPoint
            gradientType:GradientLayerType::kEaseInOut];
  _gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_gradientView];
  AddSameConstraints(_gradientView, self);

  _maskView =
      [[MultiColorGradientView alloc] initWithColors:@[
        [UIColor colorWithWhite:0 alpha:0.0],
        [UIColor colorWithWhite:0 alpha:1.0],
      ]
                                           locations:@[ @0.0, @1.0 ]
                                          startPoint:kHaloRadialStartPoint
                                            endPoint:kHaloRadialEndPoint
                                                type:kCAGradientLayerRadial];
  _maskView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  _maskView.frame = self.bounds;

  _gradientView.maskView = _maskView;
}

@end
