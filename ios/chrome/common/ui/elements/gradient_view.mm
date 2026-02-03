// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/gradient_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Returns Ease-In Ease-Out steps from `start_color` to `end_color`.
NSArray* EasedGradientColors(UIColor* start_color, UIColor* end_color) {
  NSMutableArray* colors = [NSMutableArray array];
  const int kSteps = 16;

  CGFloat red_start, green_start, blue_start, alpha_start;
  if (![start_color getRed:&red_start
                     green:&green_start
                      blue:&blue_start
                     alpha:&alpha_start]) {
    // Handle case where color isn't compatible (e.g. pattern image), fallback
    // to white
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

    float eased_progress = -(cosf(M_PI * progress) - 1.0f) / 2.0f;

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

@implementation GradientView {
  // The color at the start of the gradient.
  UIColor* _startColor;
  // The color at the end of the gradient.
  UIColor* _endColor;
  // Whether to use a eased curve for the gradient or not.
  BOOL _easedCurve;
}

#pragma mark - Public

+ (Class)layerClass {
  return [CAGradientLayer class];
}

- (instancetype)initWithStartColor:(UIColor*)startColor
                          endColor:(UIColor*)endColor
                        startPoint:(CGPoint)startPoint
                          endPoint:(CGPoint)endPoint
                     useEasedCurve:(BOOL)easedCurve {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _startColor = startColor;
    _endColor = endColor;
    _easedCurve = easedCurve;
    self.gradientLayer.startPoint = startPoint;
    self.gradientLayer.endPoint = endPoint;
    self.userInteractionEnabled = NO;
    [self updateColors];

    NSArray<UITrait>* traits = @[
      UITraitUserInterfaceIdiom.class, UITraitUserInterfaceStyle.class,
      UITraitDisplayGamut.class, UITraitAccessibilityContrast.class,
      UITraitUserInterfaceLevel.class
    ];
    __weak __typeof(self) weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      [weakSelf updateColorsOnTraitChange:previousCollection];
    };
    [self registerForTraitChanges:traits withHandler:handler];
  }
  return self;
}

- (instancetype)initWithTopColor:(UIColor*)topColor
                     bottomColor:(UIColor*)bottomColor {
  return [self initWithStartColor:topColor
                         endColor:bottomColor
                       startPoint:CGPointMake(0.5, 0)
                         endPoint:CGPointMake(0.5, 1)
                    useEasedCurve:NO];
}

- (CAGradientLayer*)gradientLayer {
  return base::apple::ObjCCastStrict<CAGradientLayer>(self.layer);
}

- (void)setStartColor:(UIColor*)startColor endColor:(UIColor*)endColor {
  _startColor = startColor;
  _endColor = endColor;
  [self updateColors];
}

#pragma mark - Private

- (void)updateColors {
  if (_easedCurve) {
    self.gradientLayer.colors = EasedGradientColors(_startColor, _endColor);
  } else {
    self.gradientLayer.colors = @[
      (id)_startColor.CGColor,
      (id)_endColor.CGColor,
    ];
  }
}

// Animate and update the view's color when its appearance has been modified via
// changes in UITraits.
- (void)updateColorsOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    [CATransaction begin];
    // If this isn't set, the changes here are automatically animated. The other
    // color changes for dark mode don't animate, however, so there ends up
    // being visual desyncing.
    [CATransaction setDisableActions:YES];
    [self updateColors];
    [CATransaction commit];
  }
}

@end
