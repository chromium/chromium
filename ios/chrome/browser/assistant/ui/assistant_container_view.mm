// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Shadow styling.
constexpr float kShadowOpacity = 0.1f;
constexpr CGFloat kShadowRadius = 10.0;
constexpr CGSize kShadowOffset = {0, 5};

// Grabber styling.
constexpr CGFloat kGrabberWidth = 33.0;
constexpr CGFloat kGrabberHeight = 4.0;
constexpr CGFloat kGrabberTopMargin = 5.0;
constexpr CGFloat kGrabberAlpha = 0.24;

}  // namespace

@implementation AssistantContainerView {
  UIView* _grabberView;
  CGFloat _cornerRadius;
  CACornerMask _maskedCorners;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _maskedCorners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner |
                     kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
    [self configureContainerStyling];
    [self setUpSubviews];
  }
  return self;
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  // Translate CACornerMask to UIRectCorner for the bezier path.
  // Mapping the active mask to `UIRectCorner` ensures the shadow perfectly
  // matches the layer's actual geometry.
  UIRectCorner rectCorners = 0;
  if (_maskedCorners & kCALayerMinXMinYCorner) {
    rectCorners |= UIRectCornerTopLeft;
  }
  if (_maskedCorners & kCALayerMaxXMinYCorner) {
    rectCorners |= UIRectCornerTopRight;
  }
  if (_maskedCorners & kCALayerMinXMaxYCorner) {
    rectCorners |= UIRectCornerBottomLeft;
  }
  if (_maskedCorners & kCALayerMaxXMaxYCorner) {
    rectCorners |= UIRectCornerBottomRight;
  }

  // Update shadow path to match the view's bounds and specific corner masking.
  self.layer.shadowPath =
      [UIBezierPath
          bezierPathWithRoundedRect:self.bounds
                  byRoundingCorners:rectCorners
                        cornerRadii:CGSizeMake(_cornerRadius, _cornerRadius)]
          .CGPath;
}

#pragma mark - Private

// Configures the visual styling of the container.
- (void)configureContainerStyling {
  self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.clipsToBounds = YES;

  self.layer.shadowColor = [UIColor blackColor].CGColor;
  self.layer.shadowOpacity = kShadowOpacity;
  self.layer.shadowOffset = kShadowOffset;
  self.layer.shadowRadius = kShadowRadius;
  self.layer.cornerRadius = _cornerRadius;
  self.layer.maskedCorners = _maskedCorners;
}

// Allows the controller to dynamically morph the container radius.
- (void)updateCornerRadius:(CGFloat)cornerRadius
             maskedCorners:(CACornerMask)maskedCorners {
  if (_cornerRadius == cornerRadius && _maskedCorners == maskedCorners) {
    return;
  }
  _cornerRadius = cornerRadius;
  _maskedCorners = maskedCorners;
  self.layer.cornerRadius = _cornerRadius;
  self.layer.maskedCorners = _maskedCorners;
  [self setNeedsLayout];
}

// Sets up the view hierarchy by creating and adding subviews.
- (void)setUpSubviews {
  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_contentView];

  _grabberView = [self createGrabberView];
  [self addSubview:_grabberView];

  [NSLayoutConstraint activateConstraints:@[
    // Grabber view constraints.
    [_grabberView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [_grabberView.topAnchor constraintEqualToAnchor:self.topAnchor
                                           constant:kGrabberTopMargin],
    [_grabberView.widthAnchor constraintEqualToConstant:kGrabberWidth],
    [_grabberView.heightAnchor constraintEqualToConstant:kGrabberHeight],
  ]];

  AddSameConstraints(_contentView, self);
}

// Creates and configures the grabber view.
- (UIView*)createGrabberView {
  UIView* grabberView = [[UIView alloc] init];
  grabberView.translatesAutoresizingMaskIntoConstraints = NO;
  grabberView.backgroundColor =
      [[UIColor blackColor] colorWithAlphaComponent:kGrabberAlpha];
  grabberView.layer.cornerRadius = kGrabberHeight / 2.0;
  return grabberView;
}

@end
