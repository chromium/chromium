// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Shadow styling.
constexpr float kShadowOpacity = 0.29f;
constexpr CGFloat kShadowRadius = 21.0;
constexpr CGSize kShadowOffset = {0, 11};

// Grabber styling.
constexpr CGFloat kGrabberWidth = 32.0;
constexpr CGFloat kGrabberHeight = 4.0;
constexpr CGFloat kGrabberTopMargin = 8.0;

}  // namespace

@implementation AssistantContainerView {
  UIView* _grabberView;
  UIView* _bottomRoundingView;
  CGFloat _topCornerRadius;
  CGFloat _bottomCornerRadius;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self configureContainerStyling];
    [self setUpSubviews];
  }
  return self;
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  self.layer.shadowPath = [self shadowPathWithTopRadius:_topCornerRadius
                                           bottomRadius:_bottomCornerRadius]
                              .CGPath;
}

#pragma mark - Private

// Generates a UIBezierPath for the container's shadow.
- (UIBezierPath*)shadowPathWithTopRadius:(CGFloat)topRadius
                            bottomRadius:(CGFloat)bottomRadius {
  UIBezierPath* path = [UIBezierPath bezierPath];
  CGFloat width = self.bounds.size.width;
  CGFloat height = self.bounds.size.height;

  [path moveToPoint:CGPointMake(0, topRadius)];
  [path addArcWithCenter:CGPointMake(topRadius, topRadius)
                  radius:topRadius
              startAngle:M_PI
                endAngle:M_PI * 1.5
               clockwise:YES];
  [path addLineToPoint:CGPointMake(width - topRadius, 0)];
  [path addArcWithCenter:CGPointMake(width - topRadius, topRadius)
                  radius:topRadius
              startAngle:M_PI * 1.5
                endAngle:0
               clockwise:YES];
  [path addLineToPoint:CGPointMake(width, height - bottomRadius)];
  [path
      addArcWithCenter:CGPointMake(width - bottomRadius, height - bottomRadius)
                radius:bottomRadius
            startAngle:0
              endAngle:M_PI * 0.5
             clockwise:YES];
  [path addLineToPoint:CGPointMake(bottomRadius, height)];
  [path addArcWithCenter:CGPointMake(bottomRadius, height - bottomRadius)
                  radius:bottomRadius
              startAngle:M_PI * 0.5
                endAngle:M_PI
               clockwise:YES];
  [path closePath];

  return path;
}

// Updates the shadow opacity based on the currently masked corners.
- (void)updateShadowOpacity {
  self.layer.shadowOpacity = (_bottomCornerRadius > 0.0) ? kShadowOpacity : 0.0;
}

// Configures the visual styling of the container.
- (void)configureContainerStyling {
  self.backgroundColor = [UIColor clearColor];
  self.clipsToBounds = NO;

  self.layer.shadowColor = [UIColor blackColor].CGColor;
  self.layer.shadowOffset = kShadowOffset;
  self.layer.shadowRadius = kShadowRadius;
  [self updateShadowOpacity];
}

// Allows the controller to dynamically morph the container radius.
- (void)updateTopCornerRadius:(CGFloat)topCornerRadius
           bottomCornerRadius:(CGFloat)bottomCornerRadius {
  if (_topCornerRadius == topCornerRadius &&
      _bottomCornerRadius == bottomCornerRadius) {
    return;
  }
  _topCornerRadius = topCornerRadius;
  _bottomCornerRadius = bottomCornerRadius;

  _contentView.layer.cornerRadius = _topCornerRadius;
  _bottomRoundingView.layer.cornerRadius = _bottomCornerRadius;

  [self updateShadowOpacity];
  [self setNeedsLayout];
}

// Sets up the view hierarchy by creating and adding subviews.
- (void)setUpSubviews {
  _bottomRoundingView = [self createBottomRoundingView];
  [self addSubview:_bottomRoundingView];

  _contentView = [self createContentView];
  [_bottomRoundingView addSubview:_contentView];

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

  AddSameConstraints(_bottomRoundingView, self);
  AddSameConstraints(_contentView, _bottomRoundingView);
}

// Creates and configures the bottom rounding clipping view.
- (UIView*)createBottomRoundingView {
  UIView* view = [[UIView alloc] init];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  view.layer.maskedCorners = kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
  view.clipsToBounds = YES;
  view.backgroundColor = [UIColor clearColor];
  return view;
}

// Creates and configures the top rounding content clipping view.
- (UIView*)createContentView {
  UIView* view = [[UIView alloc] init];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  view.layer.maskedCorners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
  view.clipsToBounds = YES;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  return view;
}

// Creates and configures the grabber view.
- (UIView*)createGrabberView {
  UIView* grabberView = [[UIView alloc] init];
  grabberView.translatesAutoresizingMaskIntoConstraints = NO;
  grabberView.backgroundColor = [UIColor colorNamed:kTertiaryBackgroundColor];
  grabberView.layer.cornerRadius = kGrabberHeight / 2.0;
  return grabberView;
}

@end
