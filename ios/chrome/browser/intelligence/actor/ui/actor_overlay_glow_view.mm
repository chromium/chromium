// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_glow_view.h"

#import <QuartzCore/QuartzCore.h>

namespace {

// The radius (thickness spread) of the inner shadow glow.
constexpr CGFloat kGlowShadowRadius = 12.0f;

// The stroke thickness used to create the single-contour shadow perimeter.
// Controls the darkness/density of the shadow: a thicker stroke provides more
// caster mass and results in a darker glow.
constexpr CGFloat kStrokePerimeterWidth = 14.0f;

}  // namespace

@implementation ActorOverlayGlowView {
  // The base color of the glow.
  UIColor* _glowColor;

  // The corner radii of the glow.
  CornerRadii _cornerRadii;

  // The layer used to mask the inner path of the glow.
  CAShapeLayer* _maskLayer;
  // The layer used to draw the shadow glow.
  CAShapeLayer* _glowShadowLayer;

  // Cached bounds from the last layout pass.
  CGRect _previousBounds;
  // True if the corner radii changed and path recalculation is needed.
  BOOL _needsPathUpdate;
}

- (instancetype)initWithGlowColor:(UIColor*)glowColor {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _glowColor = glowColor;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.userInteractionEnabled = NO;
    self.opaque = NO;
    _previousBounds = CGRectZero;
    _needsPathUpdate = YES;
    [self setupLayers];
    [self registerForTraitChanges:@[ [UITraitUserInterfaceStyle class] ]
                       withAction:@selector(updateGlowColors)];
  }
  return self;
}

- (void)setCornerRadii:(CornerRadii)cornerRadii {
  if (_cornerRadii == cornerRadii) {
    return;
  }
  _cornerRadii = cornerRadii;
  _needsPathUpdate = YES;
  [self setNeedsLayout];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  CGRect bounds = self.bounds;
  if (CGRectIsEmpty(bounds)) {
    return;
  }

  if (CGRectEqualToRect(bounds, _previousBounds) && !_needsPathUpdate) {
    return;
  }
  _previousBounds = bounds;

  [self updateLayersWithBounds:bounds];
  _needsPathUpdate = NO;
}

#pragma mark - Private

// Instantiates and sets up the mask and shadow layers.
- (void)setupLayers {
  _maskLayer = [CAShapeLayer layer];
  self.layer.mask = _maskLayer;

  _glowShadowLayer = [CAShapeLayer layer];
  _glowShadowLayer.shadowOffset = CGSizeZero;
  _glowShadowLayer.shadowRadius = kGlowShadowRadius;
  _glowShadowLayer.shadowOpacity = 1.0f;
  _glowShadowLayer.fillColor = [UIColor clearColor].CGColor;
  [self updateGlowColors];
  [self.layer addSublayer:_glowShadowLayer];
}

// Updates the paths and frames of the mask and shadow layers to match the new
// bounds.
- (void)updateLayersWithBounds:(CGRect)bounds {
  _maskLayer.frame = bounds;

  UIBezierPath* innerPath = [self createGlowPath];
  _maskLayer.path = innerPath.CGPath;

  // Convert the perimeter of the inner path into a closed ribbon stroke to
  // serve as a single-contour shadow caster without holes.
  CGPathRef strokedPath = CGPathCreateCopyByStrokingPath(
      innerPath.CGPath, nullptr, kStrokePerimeterWidth, kCGLineCapRound,
      kCGLineJoinRound, 0.0f);

  _glowShadowLayer.frame = bounds;
  _glowShadowLayer.shadowPath = strokedPath;
  CGPathRelease(strokedPath);
}

// Updates the glow colors to match the current trait collection.
- (void)updateGlowColors {
  UIColor* resolvedColor =
      [_glowColor resolvedColorWithTraitCollection:self.traitCollection];
  _glowShadowLayer.shadowColor = resolvedColor.CGColor;
}

// Creates and returns the inner bezier path for the glow.
- (UIBezierPath*)createGlowPath {
  CGRect bounds = self.bounds;
  CGFloat minX = CGRectGetMinX(bounds);
  CGFloat minY = CGRectGetMinY(bounds);
  CGFloat maxX = CGRectGetMaxX(bounds);
  CGFloat maxY = CGRectGetMaxY(bounds);

  CGMutablePathRef path = CGPathCreateMutable();
  // Start after the arc on the top left.
  CGPathMoveToPoint(path, nullptr, minX, minY + _cornerRadii.topLeft);
  // Top left corner with arc.
  CGPathAddArcToPoint(path, nullptr, minX, minY, maxX, minY,
                      _cornerRadii.topLeft);
  // Top right corner with arc.
  CGPathAddArcToPoint(path, nullptr, maxX, minY, maxX, maxY,
                      _cornerRadii.topRight);
  // Bottom right corner with arc.
  CGPathAddArcToPoint(path, nullptr, maxX, maxY, minX, maxY,
                      _cornerRadii.bottomRight);
  // Bottom left corner with arc.
  CGPathAddArcToPoint(path, nullptr, minX, maxY, minX, minY,
                      _cornerRadii.bottomLeft);
  CGPathCloseSubpath(path);

  UIBezierPath* bezierPath = [UIBezierPath bezierPathWithCGPath:path];
  CGPathRelease(path);
  return bezierPath;
}

@end
