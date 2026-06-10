// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_background_view.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Animation duration for color transition.
constexpr CGFloat kColorTransitionDuration = 0.2;

// Constants for the shadow.
constexpr CGFloat kShadowRadius = 31;
constexpr CGFloat kShadowOpacity = 0.8;
constexpr CGFloat kShadowOffset = 13;
// Adds the cutout shape (arcs and line) to the path.
// Assumes the path is already at the starting point (bounds.size.width, 0).
void AddCutoutToPath(UIBezierPath* path, CGRect bounds) {
  // Right inverse rounded corner.
  [path addArcWithCenter:CGPointMake(bounds.size.width - kAppBarCornerRadius, 0)
                  radius:kAppBarCornerRadius
              startAngle:0
                endAngle:M_PI_2
               clockwise:YES];

  [path addLineToPoint:CGPointMake(kAppBarCornerRadius, kAppBarCornerRadius)];

  // Left inverse rounded corner.
  [path addArcWithCenter:CGPointMake(kAppBarCornerRadius, 0)
                  radius:kAppBarCornerRadius
              startAngle:M_PI_2
                endAngle:M_PI
               clockwise:YES];
}

}  // namespace

@implementation AppBarBackgroundView {
  CAShapeLayer* _maskLayer;
  UIBezierPath* _maskPath;
  CGRect _lastBounds;
  CAShapeLayer* _shadowLayer;
  UIVisualEffectView* _blurView;
  // Shape layer used to render the background color. This allows the view's own
  // background color to remain transparent/clear, preventing EarlGrey from
  // incorrectly identifying the entire bounding box of the view as opaque and
  // blocking visibility of underlying elements in tests.
  CAShapeLayer* _backgroundShapeLayer;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.opaque = NO;
    if (!IsFullscreenRefactoringEnabled()) {
      _backgroundShapeLayer = [CAShapeLayer layer];
      [self.layer insertSublayer:_backgroundShapeLayer atIndex:0];
    }

    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                       withAction:@selector(updateBackgroundColor)];

    _maskLayer = [CAShapeLayer layer];
    _maskLayer.fillRule = kCAFillRuleEvenOdd;
    self.layer.mask = _maskLayer;

    if (IsFullscreenRefactoringEnabled()) {
      UIBlurEffect* blurEffect =
          [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterialDark];
      _blurView = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
      [self addSubview:_blurView];
    }

    _shadowLayer = [CAShapeLayer layer];
    [self.layer addSublayer:_shadowLayer];

    [self updateBackgroundColor];
    [self updateMask];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  if (_blurView) {
    _blurView.frame = self.bounds;
  }
  [self updateMask];
}

#pragma mark - UIView

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  // For the background area, only return YES if the point is within the filled
  // part of the mask (the "ears" and the main bar).
  return [_maskPath containsPoint:point];
}

- (void)setIncognito:(BOOL)incognito {
  if (_incognito == incognito) {
    return;
  }
  _incognito = incognito;
  [UIView animateWithDuration:kColorTransitionDuration
                   animations:^{
                     [self updateBackgroundColor];
                   }];
}

- (void)setHideColorBackground:(BOOL)hideColorBackground {
  if (_hideColorBackground == hideColorBackground) {
    return;
  }
  CAShapeLayer* shadowLayer = _shadowLayer;
  UIVisualEffectView* blurView = _blurView;
  _hideColorBackground = hideColorBackground;

  if (!hideColorBackground && blurView) {
    blurView.hidden = NO;
  }

  [UIView animateWithDuration:kColorTransitionDuration
      animations:^{
        [self updateBackgroundColor];
        shadowLayer.opacity = hideColorBackground ? 0 : 1;
        if (blurView) {
          blurView.alpha = hideColorBackground ? 0 : 1;
        }
      }
      completion:^(BOOL finished) {
        if (hideColorBackground && blurView) {
          blurView.hidden = YES;
        }
      }];
}

#pragma mark - Private

// Updates the background color of the app bar.
- (void)updateBackgroundColor {
  if (IsFullscreenRefactoringEnabled()) {
    return;
  }
  UIColor* color = [UIColor colorNamed:kAppBarColor];
  if (self.hideColorBackground) {
    color = [UIColor clearColor];
  } else if (self.incognito) {
    color = [UIColor colorNamed:kAppBarIncognitoColor];
  }
  _backgroundShapeLayer.fillColor = color.CGColor;
}

// Updates the cut out mask if the shape of the app bar changed.
- (void)updateMask {
  CGRect bounds = self.bounds;
  if (CGRectIsEmpty(bounds) || CGRectEqualToRect(bounds, _lastBounds)) {
    return;
  }

  _lastBounds = bounds;

  // Use a positive path to construct the mask instead of subtracting shapes.
  _maskPath = [UIBezierPath bezierPath];
  [_maskPath moveToPoint:CGPointMake(0, bounds.size.height)];
  [_maskPath addLineToPoint:CGPointMake(bounds.size.width, bounds.size.height)];
  [_maskPath addLineToPoint:CGPointMake(bounds.size.width, 0)];

  AddCutoutToPath(_maskPath, bounds);

  [_maskPath closePath];

  _maskLayer.path = _maskPath.CGPath;
  _backgroundShapeLayer.path = _maskPath.CGPath;

  // Inner shadow implementation.
  // Create a path that is specifically above the top cutout edge.
  UIBezierPath* shadowSourcePath = [UIBezierPath bezierPath];
  [shadowSourcePath moveToPoint:CGPointMake(bounds.size.width, 0)];

  AddCutoutToPath(shadowSourcePath, bounds);

  // Now close the path by going up and around above the view bounds.
  [shadowSourcePath addLineToPoint:CGPointMake(0, -kShadowRadius * 2)];
  [shadowSourcePath
      addLineToPoint:CGPointMake(bounds.size.width, -kShadowRadius * 2)];
  [shadowSourcePath closePath];

  _shadowLayer.shadowColor = [UIColor blackColor].CGColor;
  _shadowLayer.shadowOpacity = kShadowOpacity;
  _shadowLayer.shadowOffset = CGSizeMake(0, kShadowOffset);
  _shadowLayer.shadowRadius = kShadowRadius;
  _shadowLayer.shadowPath = shadowSourcePath.CGPath;
}

@end
