// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbar_background_view.h"

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_blur.h"
#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// TODO(crbug.com/483974847): Understand why this is not using a standard color.
constexpr int kGradientBackgroundColor = 0x2F312B;

// The distance at the start of which the background starts dimming out.
constexpr CGFloat kScrollToEdgeAlphaUpdateDistance = 60;

// The extra distance the lowest background should extend to.
constexpr CGFloat kLowestBackgroundExtraDistance = 60;

// The percentage of the blur effect.
constexpr CGFloat kBlurEffectPercentage = 0.1;

// The length of the blur effect.
constexpr CGFloat kBlurLength = 0.2;

// The length of the lowest background gradient.
constexpr CGFloat kLowestBackgroundGradientLength = 0.77;

}  // namespace

@implementation TabGridToolbarBackgroundView {
  // The background view standing at the bottom.
  UIView* _lowestBackground;
  // The blur, covering _lowestBackground.
  UIView* _blurBackground;
  // The background view standing at the top, covering _blurBackground.
  UIView* _topBackground;
}

- (instancetype)initWithPosition:(TabGridToolbarBackgroundPosition)position {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    BOOL isTop = position == TabGridToolbarBackgroundPosition::kTop;

    _lowestBackground = [self createLowestBackgroundForTop:isTop];
    [self addSubview:_lowestBackground];
    AddSameConstraintsWithInsets(
        self, _lowestBackground,
        NSDirectionalEdgeInsetsMake(
            isTop ? 0 : kLowestBackgroundExtraDistance, 0,
            isTop ? kLowestBackgroundExtraDistance : 0, 0));

    _blurBackground = [self createBlurBackgroundForTop:isTop];
    [self addSubview:_blurBackground];
    AddSameConstraints(self, _blurBackground);

    _topBackground = [self createTopBackgroundForTop:isTop];
    [self addSubview:_topBackground];
    AddSameConstraints(self, _topBackground);
  }
  return self;
}

- (void)setRemainingScrollDistance:(CGFloat)remainingScrollDistance {
  _remainingScrollDistance = remainingScrollDistance;
  _lowestBackground.alpha =
      remainingScrollDistance / kScrollToEdgeAlphaUpdateDistance;
}

#pragma mark - Private

// Creates the lowest background view.
- (UIView*)createLowestBackgroundForTop:(BOOL)isTop {
  CGPoint lowestStartPoint = isTop ? CGPointMake(0.5, 1) : CGPointMake(0.5, 0);
  CGPoint lowestEndPoint =
      isTop ? CGPointMake(0.5, 1 - kLowestBackgroundGradientLength)
            : CGPointMake(0.5, kLowestBackgroundGradientLength);
  UIView* lowestBackground = [[GradientView alloc]
      initWithStartColor:UIColor.clearColor
                endColor:UIColorFromRGB(kGradientBackgroundColor)
              startPoint:lowestStartPoint
                endPoint:lowestEndPoint
            gradientType:GradientLayerType::kEaseInThenLinear];
  lowestBackground.translatesAutoresizingMaskIntoConstraints = NO;
  return lowestBackground;
}

// Creates the blur background view.
- (UIView*)createBlurBackgroundForTop:(BOOL)isTop {
  CGPoint blurStartPoint =
      isTop ? CGPointMake(0.5, 1 - kBlurLength) : CGPointMake(0.5, kBlurLength);
  CGPoint blurEndPoint = isTop ? CGPointMake(0.5, 1) : CGPointMake(0.5, 0);
  UIBlurEffect* targetEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemChromeMaterialDark];
  UIView* blurBackground =
      [[GradientBlur alloc] initWithEffect:targetEffect
                          effectPercentage:kBlurEffectPercentage
                                startPoint:blurStartPoint
                                  endPoint:blurEndPoint
                              gradientType:GradientLayerType::kLinear];
  blurBackground.translatesAutoresizingMaskIntoConstraints = NO;
  return blurBackground;
}

// Creates the top background view.
- (UIView*)createTopBackgroundForTop:(BOOL)isTop {
  CGPoint topStartPoint = isTop ? CGPointMake(0.5, 0) : CGPointMake(0.5, 1);
  CGPoint topEndPoint = isTop ? CGPointMake(0.5, 1) : CGPointMake(0.5, 0);
  UIView* topBackground = [[GradientView alloc]
      initWithStartColor:[UIColor.blackColor colorWithAlphaComponent:0.5]
                endColor:UIColor.clearColor
              startPoint:topStartPoint
                endPoint:topEndPoint
            gradientType:GradientLayerType::kLinear];
  topBackground.translatesAutoresizingMaskIntoConstraints = NO;
  return topBackground;
}

@end
