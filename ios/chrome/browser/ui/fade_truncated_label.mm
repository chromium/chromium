// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fade_truncated_label.h"

#include <algorithm>

#import "ios/chrome/browser/ui/util/animation_util.h"
#import "ios/chrome/browser/ui/util/reversed_animation.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Animation key used for frame animations.
NSString* const kFadeTruncatedLabelAnimationKey =
    @"FadeTruncatedLabelAnimationKey";
}

@interface FadeTruncatedLabel ()

// Layer used to apply fade truncation to label.
@property(nonatomic, strong) CAGradientLayer* maskLayer;

// Temporary label used during animations.
@property(nonatomic, strong) UILabel* animationLabel;

// Returns the percentage of the label's width at which to begin the fade
// gradient.
+ (CGFloat)gradientPercentageForText:(NSString*)text
                      withAttributes:(NSDictionary*)attributes
                            inBounds:(CGRect)bounds;

@end

@implementation FadeTruncatedLabel

@synthesize maskLayer = _maskLayer;
@synthesize animationLabel = _animationLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Set background color and line break mode.
    self.backgroundColor = [UIColor clearColor];
    self.lineBreakMode = NSLineBreakByClipping;

    // Instantiate |maskLayer| and add as mask.
    self.maskLayer = [[self class] maskLayerForText:self.text
                                     withAttributes:@{
                                       NSFontAttributeName : self.font
                                     }
                                           inBounds:{CGPointZero, frame.size}];
    self.layer.mask = self.maskLayer;
  }
  return self;
}

#pragma mark - UILabel overrides

- (void)drawTextInRect:(CGRect)rect {
  // Draw if not animating.
  if (!self.animationLabel)
    [super drawTextInRect:rect];

  // Update the mask gradient.
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  self.maskLayer.frame = rect;
  NSDictionary* fontAttributes = @{NSFontAttributeName : self.font};
  CGFloat gradientBeginPercentage =
      [[self class] gradientPercentageForText:self.text
                               withAttributes:fontAttributes
                                     inBounds:self.maskLayer.bounds];
  self.maskLayer.locations = @[ @(gradientBeginPercentage), @(1.0) ];
  [CATransaction commit];
}

#pragma mark - Animations

- (void)animateFromBeginFrame:(CGRect)beginFrame
                   toEndFrame:(CGRect)endFrame
                     duration:(CFTimeInterval)duration
               timingFunction:(CAMediaTimingFunction*)timingFunction {
  CAAnimation* frameAnimation = nil;

  // Animate the label.
  frameAnimation = FrameAnimationMake(self.layer, beginFrame, endFrame);
  frameAnimation.duration = duration;
  frameAnimation.timingFunction = timingFunction;
  [self.layer addAnimation:frameAnimation
                    forKey:kFadeTruncatedLabelAnimationKey];

  // When animating, add a temporary label using the larger frame size so that
  // truncation can occur via the gradient rather than by the edge of the text
  // layer's backing store.
  CGSize animationTextSize =
      CGSizeMake(std::max(beginFrame.size.width, endFrame.size.width),
                 std::max(beginFrame.size.height, endFrame.size.height));
  self.animationLabel =
      [[UILabel alloc] initWithFrame:{CGPointZero, animationTextSize}];
  self.animationLabel.text = self.text;
  self.animationLabel.textColor = self.textColor;
  self.animationLabel.font = self.font;
  [self addSubview:self.animationLabel];

  // Animate the mask layer.
  CGRect beginBounds = {CGPointZero, beginFrame.size};
  CGRect endBounds = {CGPointZero, endFrame.size};
  frameAnimation = FrameAnimationMake(self.maskLayer, beginBounds, endBounds);
  frameAnimation.duration = duration;
  frameAnimation.timingFunction = timingFunction;
  NSDictionary* attributes = @{NSFontAttributeName : self.font};
  CGFloat beginGradientPercentage =
      [[self class] gradientPercentageForText:self.text
                               withAttributes:attributes
                                     inBounds:beginBounds];
  CGFloat endGradientPercentage =
      [[self class] gradientPercentageForText:self.text
                               withAttributes:attributes
                                     inBounds:endBounds];
  CABasicAnimation* gradientAnimation =
      [CABasicAnimation animationWithKeyPath:@"locations"];
  gradientAnimation.fromValue = @[ @(beginGradientPercentage), @(1.0) ];
  gradientAnimation.toValue = @[ @(endGradientPercentage), @(1.0) ];
  gradientAnimation.duration = duration;
  gradientAnimation.timingFunction = timingFunction;
  CAAnimation* animation =
      AnimationGroupMake(@[ frameAnimation, gradientAnimation ]);
  [self.maskLayer addAnimation:animation
                        forKey:kFadeTruncatedLabelAnimationKey];
}

- (void)reverseAnimations {
  // Reverse the animations, but leave the animation label in place.
  ReverseAnimationsForKeyForLayers(kFadeTruncatedLabelAnimationKey,
                                   @[ self.layer, self.maskLayer ]);
}

- (void)cleanUpAnimations {
  // Remove animation label and redraw.
  [self.animationLabel removeFromSuperview];
  self.animationLabel = nil;
  [self setNeedsDisplay];
  // Remove animations from layers.
  RemoveAnimationForKeyFromLayers(kFadeTruncatedLabelAnimationKey,
                                  @[ self.layer, self.maskLayer ]);
}

#pragma mark - Class methods

+ (CGFloat)gradientPercentageForText:(NSString*)text
                      withAttributes:(NSDictionary*)attributes
                            inBounds:(CGRect)bounds {
  CGSize textSize =
      ui::AlignSizeToUpperPixel([text sizeWithAttributes:attributes]);
  CGFloat gradientBeginPercentage = 1.0;
  if (textSize.width > bounds.size.width) {
    // Fade width is chosen to match GTMFadeTruncatingLabel.
    CGFloat fadeWidth =
        std::min<CGFloat>(bounds.size.height * 2, floor(bounds.size.width / 4));
    gradientBeginPercentage =
        (bounds.size.width - fadeWidth) / bounds.size.width;
  }
  return gradientBeginPercentage;
}

+ (CAGradientLayer*)maskLayerForText:(NSString*)text
                      withAttributes:(NSDictionary*)attributes
                            inBounds:(CGRect)bounds {
  CAGradientLayer* maskLayer = [CAGradientLayer layer];
  maskLayer.bounds = bounds;
  maskLayer.colors = @[
    reinterpret_cast<id>([UIColor blackColor].CGColor),
    reinterpret_cast<id>([UIColor clearColor].CGColor)
  ];
  maskLayer.startPoint = CGPointMake(0.0, 0.5);
  maskLayer.endPoint = CGPointMake(1.0, 0.5);
  CGFloat gradientBeginPercentage = [self gradientPercentageForText:text
                                                     withAttributes:attributes
                                                           inBounds:bounds];
  maskLayer.locations = @[ @(gradientBeginPercentage), @(1.0) ];
  return maskLayer;
}

@end
