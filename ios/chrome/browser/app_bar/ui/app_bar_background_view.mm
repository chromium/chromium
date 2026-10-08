// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_background_view.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Animation duration for color transition.
constexpr CGFloat kColorTransitionDuration = 0.2;

// Constants for the shadow.
constexpr CGFloat kLightShadowRadius = 47;
constexpr CGFloat kDarkShadowRadius = 31;
constexpr CGFloat kLightShadowOpacity = 0.15;
constexpr CGFloat kDarkShadowOpacity = 0.8;
constexpr CGFloat kLightShadowOffset = 15;
constexpr CGFloat kDarkShadowOffset = 13;

// Adds the cutout shape (arcs and line) to the path.
// Assumes the path is already at the starting point (bounds.size.width,
// y_offset).
void AddCutoutToPath(UIBezierPath* path,
                     CGRect bounds,
                     CGFloat corner_radius,
                     CGFloat y_offset) {
  // Right inverse rounded corner.
  [path
      addArcWithCenter:CGPointMake(bounds.size.width - corner_radius, y_offset)
                radius:corner_radius
            startAngle:0
              endAngle:M_PI_2
             clockwise:YES];

  [path addLineToPoint:CGPointMake(corner_radius, y_offset + corner_radius)];

  // Left inverse rounded corner.
  [path addArcWithCenter:CGPointMake(corner_radius, y_offset)
                  radius:corner_radius
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

  // The blur view (when one is used).
  UIVisualEffectView* _blurView;
  // Shape layer used to render the background color. This allows the view's own
  // background color to remain transparent/clear, preventing EarlGrey from
  // incorrectly identifying the entire bounding box of the view as opaque and
  // blocking visibility of underlying elements in tests.
  CAShapeLayer* _backgroundShapeLayer;
  BOOL _animatingCornerRadius;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Initialize with the default corner radius to ensure the cutout path draws
    // correctly on the first layout pass, avoiding a flat edge or visual jump.
    _cornerRadius = kAppBarCornerRadius;
    self.opaque = NO;
    if (!IsFullscreenRefactoringEnabled()) {
      _backgroundShapeLayer = [CAShapeLayer layer];
      [self.layer insertSublayer:_backgroundShapeLayer atIndex:0];
    }

    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                       withAction:@selector(updateForUserInterfaceStyle)];

    _maskLayer = [CAShapeLayer layer];
    _maskLayer.fillRule = kCAFillRuleEvenOdd;
    self.layer.mask = _maskLayer;

    if (IsFullscreenRefactoringEnabled()) {
      _blurView = [[UIVisualEffectView alloc] initWithEffect:nil];
      [self updateBlur];
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

#pragma mark - Properties

- (void)setIncognito:(BOOL)incognito {
  if (_incognito == incognito) {
    return;
  }
  _incognito = incognito;
  [UIView animateWithDuration:kColorTransitionDuration
                   animations:^{
                     [self updateBackgroundColor];
                     [self updateBlur];
                     [self updateShadow];
                   }];
}

- (void)setHideColorBackground:(BOOL)hideColorBackground {
  if (_hideColorBackground == hideColorBackground) {
    return;
  }
  UIVisualEffectView* blurView = _blurView;
  _hideColorBackground = hideColorBackground;

  if (!hideColorBackground) {
    blurView.hidden = NO;
  }

  [UIView animateWithDuration:kColorTransitionDuration
      animations:^{
        [self updateBackgroundColor];
        [self updateBlur];
        [self updateShadow];
      }
      completion:^(BOOL finished) {
        if (hideColorBackground) {
          blurView.hidden = YES;
        }
      }];
}

- (void)setCornerRadius:(CGFloat)cornerRadius {
  if (!IsCornerRadiusChangeSignificant(_cornerRadius, cornerRadius)) {
    return;
  }
  _cornerRadius = cornerRadius;
  _lastBounds = CGRectZero;

  if ([UIView inheritedAnimationDuration] > 0) {
    _animatingCornerRadius = YES;
  }

  [self updateMask];
}

#pragma mark - Private

// Updates the blur effect of the app bar.
- (void)updateBlur {
  if (_hideColorBackground) {
    _blurView.effect = nil;
    return;
  }

  UIBlurEffectStyle blurStyle;
  if (IsLightAppBarEnabled()) {
    blurStyle = _incognito ? UIBlurEffectStyleSystemMaterialDark
                           : UIBlurEffectStyleSystemMaterial;
  } else {
    blurStyle = UIBlurEffectStyleSystemMaterialDark;
  }
  _blurView.effect = [UIBlurEffect effectWithStyle:blurStyle];
}

// Updates for user interface style changes.
- (void)updateForUserInterfaceStyle {
  [self updateBackgroundColor];
  [self updateShadow];
}

// Updates the shadow of the app bar.
- (void)updateShadow {
  if (!_incognito &&
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleLight &&
      IsLightAppBarEnabled()) {
    _shadowLayer.opacity = _hideColorBackground ? 0 : kLightShadowOpacity;
    _shadowLayer.shadowOffset = CGSizeMake(0, kLightShadowOffset);
    _shadowLayer.shadowRadius = kLightShadowRadius;
  } else {
    _shadowLayer.opacity = _hideColorBackground ? 0 : kDarkShadowOpacity;
    _shadowLayer.shadowOffset = CGSizeMake(0, kDarkShadowOffset);
    _shadowLayer.shadowRadius = kDarkShadowRadius;
  }
}

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

  CGFloat yOffset = kAppBarCornerRadiusMax - self.cornerRadius;

  // Use a positive path to construct the mask instead of subtracting shapes.
  _maskPath = [UIBezierPath bezierPath];
  [_maskPath moveToPoint:CGPointMake(0, bounds.size.height)];
  [_maskPath addLineToPoint:CGPointMake(bounds.size.width, bounds.size.height)];
  [_maskPath addLineToPoint:CGPointMake(bounds.size.width, yOffset)];

  AddCutoutToPath(_maskPath, bounds, self.cornerRadius, yOffset);

  [_maskPath closePath];

  CGPathRef oldMaskPath = _maskLayer.path;
  NSTimeInterval duration = [UIView inheritedAnimationDuration];

  // Only animate the path changes when the corner radius is explicitly updated.
  // When bounds change during general layout passes, update the path instantly
  // to follow the layout smoothly without generating competing CA animations.
  if (_animatingCornerRadius && duration > 0 && oldMaskPath) {
    [self animatePathChangeInLayer:_maskLayer
                          fromPath:oldMaskPath
                            toPath:_maskPath.CGPath
                           keyPath:@"path"
                          duration:duration];
    [self animatePathChangeInLayer:_backgroundShapeLayer
                          fromPath:oldMaskPath
                            toPath:_maskPath.CGPath
                           keyPath:@"path"
                          duration:duration];
  }

  _maskLayer.path = _maskPath.CGPath;
  _backgroundShapeLayer.path = _maskPath.CGPath;

  // Inner shadow implementation.
  // Create a path that is specifically above the top cutout edge.
  UIBezierPath* shadowSourcePath = [UIBezierPath bezierPath];
  [shadowSourcePath moveToPoint:CGPointMake(bounds.size.width, yOffset)];

  AddCutoutToPath(shadowSourcePath, bounds, self.cornerRadius, yOffset);

  // Now close the path by going up and around above the view bounds.
  [shadowSourcePath
      addLineToPoint:CGPointMake(0, yOffset - kLightShadowRadius * 2)];
  [shadowSourcePath
      addLineToPoint:CGPointMake(bounds.size.width,
                                 yOffset - kLightShadowRadius * 2)];
  [shadowSourcePath closePath];

  CGPathRef oldShadowPath = _shadowLayer.shadowPath;
  if (_animatingCornerRadius && duration > 0 && oldShadowPath) {
    [self animatePathChangeInLayer:_shadowLayer
                          fromPath:oldShadowPath
                            toPath:shadowSourcePath.CGPath
                           keyPath:@"shadowPath"
                          duration:duration];
  }

  _animatingCornerRadius = NO;

  _shadowLayer.shadowColor = [UIColor blackColor].CGColor;
  _shadowLayer.shadowOpacity = 1;
  _shadowLayer.shadowPath = shadowSourcePath.CGPath;
  [self updateShadow];
}

- (void)animatePathChangeInLayer:(CAShapeLayer*)layer
                        fromPath:(CGPathRef)fromPath
                          toPath:(CGPathRef)toPath
                         keyPath:(NSString*)keyPath
                        duration:(NSTimeInterval)duration {
  CASpringAnimation* pathAnim =
      [CASpringAnimation animationWithKeyPath:keyPath];
  pathAnim.duration = duration;
  pathAnim.fromValue = (__bridge id)fromPath;
  pathAnim.toValue = (__bridge id)toPath;
  pathAnim.stiffness = kAssistantSheetSpringStiffness;
  pathAnim.damping = kAssistantSheetSpringDampingValue;

  [layer addAnimation:pathAnim forKey:keyPath];
}

@end
