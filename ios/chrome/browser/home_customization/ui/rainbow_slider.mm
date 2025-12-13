// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/rainbow_slider.h"

namespace {
// The thickness of the slider's gradient track layer.
const CGFloat kTrackThickness = 13.0;

// The corner radius applied to the gradient track layer.
const CGFloat kTrackCornerRadius = 8.0;

// The diameter of the custom thumb image.
const CGFloat kThumbDiameter = 33.0;

// The thickness of the white border around the thumb circle.
const CGFloat kThumbBorderWidth = 3.0;

// The blur radius for the thumb shadow.
const CGFloat kThumbShadowBlur = 3.0;

// The opacity of the thumb shadow.
const CGFloat kThumbShadowOpacity = 0.3;

// The offset of the thumb shadow.
const CGSize kThumbShadowOffset = {0.0, 1.0};
}  // namespace

@interface RainbowSlider () {
  // The layer used to display the rainbow gradient underneath the slider track.
  CAGradientLayer* _gradientLayer;
}

@end

@implementation RainbowSlider

- (UIImage*)thumbImageWithColor:(UIColor*)color diameter:(CGFloat)diameter {
  // Constants remain the same, ensuring canvas is large enough for the shadow.
  const CGFloat kShadowHorizontalPadding =
      kThumbShadowBlur + kThumbShadowOffset.width;
  const CGFloat kShadowVerticalPadding =
      kThumbShadowBlur + kThumbShadowOffset.height;

  CGSize size = CGSizeMake(diameter + 2 * kShadowHorizontalPadding,
                           diameter + 2 * kShadowVerticalPadding);

  CGRect thumbRect = CGRectMake(kShadowHorizontalPadding,
                                kShadowVerticalPadding, diameter, diameter);

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:size];

  UIImage* thumb = [renderer
      imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
        CGContextRef ctx = rendererContext.CGContext;
        CGContextSetAllowsAntialiasing(ctx, true);
        CGContextSetShouldAntialias(ctx, true);

        // Creates the shadow.
        CGContextSetShadowWithColor(
            ctx, kThumbShadowOffset, kThumbShadowBlur,
            [UIColor colorWithWhite:0.0 alpha:kThumbShadowOpacity].CGColor);

        // Draw a black circle (which is opaque and casts a strong shadow).
        [[UIColor blackColor] setFill];
        UIBezierPath* shadowCasterPath =
            [UIBezierPath bezierPathWithOvalInRect:thumbRect];
        [shadowCasterPath fill];

        // Clear the black circle itself, leaving ONLY the shadow.
        CGContextSetBlendMode(ctx, kCGBlendModeClear);
        [shadowCasterPath fill];  // Use the same path to clear the area.
        CGContextSetBlendMode(ctx, kCGBlendModeNormal);
        CGContextSetShadowWithColor(ctx, CGSizeZero, 0.0, nullptr);

        // White border (Drawn without shadow).
        UIBezierPath* outerPath =
            [UIBezierPath bezierPathWithOvalInRect:thumbRect];
        [[UIColor whiteColor] setFill];
        [outerPath fill];

        // Inner circle (Drawn without shadow).
        CGRect innerRect =
            CGRectInset(thumbRect, kThumbBorderWidth, kThumbBorderWidth);
        UIBezierPath* innerPath =
            [UIBezierPath bezierPathWithOvalInRect:innerRect];
        [color setFill];
        [innerPath fill];
      }];

  return thumb;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];

  if (self) {
    _gradientLayer = [CAGradientLayer layer];
    // Hues taken from the color picker specifications.
    _gradientLayer.colors = @[
      (id)[UIColor colorWithHue:0.00 saturation:1.0 brightness:1.0 alpha:1.0]
          .CGColor,
      (id)[UIColor colorWithHue:0.17 saturation:1.0 brightness:1.0 alpha:1.0]
          .CGColor,
      (id)[UIColor colorWithHue:0.35 saturation:1.0 brightness:1.0 alpha:1.0]
          .CGColor,
      (id)[UIColor colorWithHue:0.51 saturation:1.0 brightness:1.0 alpha:1.0]
          .CGColor,
      (id)[UIColor colorWithHue:0.68 saturation:1.0 brightness:1.0 alpha:1.0]
          .CGColor,
      (id)[UIColor colorWithHue:0.83 saturation:1.0 brightness:1.0 alpha:1.0]
          .CGColor,
      (id)[UIColor colorWithHue:1.00 saturation:1.0 brightness:1.0 alpha:1.0]
          .CGColor
    ];

    _gradientLayer.startPoint = CGPointMake(0.0, 0.5);
    _gradientLayer.endPoint = CGPointMake(1.0, 0.5);
    _gradientLayer.cornerRadius = kTrackCornerRadius;

    self.minimumTrackTintColor = [UIColor clearColor];
    self.maximumTrackTintColor = [UIColor clearColor];

    [self addTarget:self
                  action:@selector(updateThumbColor)
        forControlEvents:UIControlEventValueChanged];

    [self.layer insertSublayer:_gradientLayer atIndex:0];
    [self updateThumbColor];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  CGRect trackFrame = [self trackRectForBounds:self.bounds];
  _gradientLayer.frame = trackFrame;
}

#pragma mark - UISlider

- (CGRect)trackRectForBounds:(CGRect)bounds {
  return CGRectMake(
      bounds.origin.x,
      bounds.origin.y + (bounds.size.height - kTrackThickness) / 2.0,
      bounds.size.width, kTrackThickness);
}

- (void)setMinimumValue:(float)minimumValue {
  // Minimum value is fixed to UISlider's default (0.0); ignore overrides.
}

- (void)setMaximumValue:(float)maximumValue {
  // Maximum value is fixed to UISlider's default (1.0); ignore overrides.
}

- (void)setValue:(float)value {
  [super setValue:value];
  [self updateThumbColor];
}

#pragma mark - Helper functions

- (void)setColor:(UIColor*)color {
  CGFloat hue = 0.0;
  CGFloat saturation = 0.0;
  CGFloat brightness = 0.0;
  CGFloat alpha = 0.0;

  [color getHue:&hue
      saturation:&saturation
      brightness:&brightness
           alpha:&alpha];

  [self setValue:hue];
}

#pragma mark - Private

// Calculates the color based on the current value and sets the thumb image.
- (void)updateThumbColor {
  UIColor* thumbColor = [UIColor colorWithHue:self.value
                                   saturation:1.0
                                   brightness:1.0
                                        alpha:1.0];
  UIImage* thumb = [self thumbImageWithColor:thumbColor
                                    diameter:kThumbDiameter];
  [self setThumbImage:thumb forState:UIControlStateNormal];
}

@end
