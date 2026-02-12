// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_layer.h"

@implementation GradientView {
  // The color at the start of the gradient.
  UIColor* _startColor;
  // The color at the end of the gradient.
  UIColor* _endColor;
}

#pragma mark - Public

+ (Class)layerClass {
  return [GradientLayer class];
}

- (instancetype)initWithStartColor:(UIColor*)startColor
                          endColor:(UIColor*)endColor
                        startPoint:(CGPoint)startPoint
                          endPoint:(CGPoint)endPoint
                      gradientType:(GradientLayerType)gradientType {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _startColor = startColor;
    _endColor = endColor;
    self.gradientLayer.gradientType = gradientType;
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
                     gradientType:GradientLayerType::kLinear];
}

- (GradientLayer*)gradientLayer {
  return base::apple::ObjCCastStrict<GradientLayer>(self.layer);
}

- (void)setStartColor:(UIColor*)startColor endColor:(UIColor*)endColor {
  _startColor = startColor;
  _endColor = endColor;
  [self updateColors];
}

#pragma mark - Private

- (void)updateColors {
  [self.gradientLayer setStartColor:_startColor endColor:_endColor];
}

// Animates and updates the view's color when its appearance has been modified
// via changes in UITraits.
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
