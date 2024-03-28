// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/gradient_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface GradientView ()

// The color at the start of the gradient.
@property(nonatomic, strong) UIColor* startColor;
// The color at the end of the gradient.
@property(nonatomic, strong) UIColor* endColor;

@end

@implementation GradientView

#pragma mark - Public

+ (Class)layerClass {
  return [CAGradientLayer class];
}

- (instancetype)initWithStartColor:(UIColor*)startColor
                          endColor:(UIColor*)endColor
                        startPoint:(CGPoint)startPoint
                          endPoint:(CGPoint)endPoint {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.startColor = startColor;
    self.endColor = endColor;
    self.gradientLayer.startPoint = startPoint;
    self.gradientLayer.endPoint = endPoint;
    self.userInteractionEnabled = NO;
    [self updateColors];
  }
  return self;
}

- (instancetype)initWithTopColor:(UIColor*)topColor
                     bottomColor:(UIColor*)bottomColor {
  return [self initWithStartColor:topColor
                         endColor:bottomColor
                       startPoint:CGPointMake(0.5, 0)
                         endPoint:CGPointMake(0.5, 1)];
}

- (CAGradientLayer*)gradientLayer {
  return base::apple::ObjCCastStrict<CAGradientLayer>(self.layer);
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
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

- (void)setStartColor:(UIColor*)startColor endColor:(UIColor*)endColor {
  self.startColor = startColor;
  self.endColor = endColor;
  [self updateColors];
}

#pragma mark - Private

- (void)updateColors {
  self.gradientLayer.colors = @[
    (id)self.startColor.CGColor,
    (id)self.endColor.CGColor,
  ];
}

@end
