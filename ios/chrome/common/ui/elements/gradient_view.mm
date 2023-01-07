// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/gradient_view.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GradientView ()

// The color at the top of the gradient.
@property(nonatomic, strong) UIColor* topColor;
// The color at the bottom of the gradient.
@property(nonatomic, strong) UIColor* bottomColor;

@end

@implementation GradientView

#pragma mark - Public

+ (Class)layerClass {
  return [CAGradientLayer class];
}

- (instancetype)initWithTopColor:(UIColor*)topColor
                     bottomColor:(UIColor*)bottomColor {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.topColor = topColor;
    self.bottomColor = bottomColor;
    self.userInteractionEnabled = NO;
    [self updateColors];
  }
  return self;
}

- (CAGradientLayer*)gradientLayer {
  return base::mac::ObjCCastStrict<CAGradientLayer>(self.layer);
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    [self updateColors];
  }
}

#pragma mark - Private

- (void)updateColors {
  [CATransaction begin];
  // If this isn't set, the changes here are automatically animated. The other
  // color changes for dark mode don't animate, however, so there ends up being
  // visual desyncing.
  [CATransaction setDisableActions:YES];

  self.gradientLayer.colors = @[
    (id)self.topColor.CGColor,
    (id)self.bottomColor.CGColor,
  ];
  [CATransaction commit];
}

@end
