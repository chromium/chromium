// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_view.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_constants.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"
#import "ios/chrome/browser/assistant/ui/assistant_grabber_button.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Grabber styling.
constexpr CGFloat kGrabberWidth = 32.0;
constexpr CGFloat kGrabberHeight = 4.0;
constexpr CGFloat kGrabberTopMargin = 8.0;

// Shadow styling.
const float kAssistantShadowOpacity = 0.16f;
const CGFloat kAssistantShadowRadius = 20.0;
const CGSize kAssistantShadowOffset = {0, 8};

}  // namespace

@implementation AssistantContainerView {
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
  if (IsIPhoneLandscapeLayout(self.traitCollection)) {
    self.layer.shadowOpacity = kAssistantShadowOpacity;
    return;
  }
  self.layer.shadowOpacity =
      (_bottomCornerRadius > 0.0) ? kAssistantShadowOpacity : 0.0;
}

// Configures the visual styling of the container.
- (void)configureContainerStyling {
  self.backgroundColor = [UIColor clearColor];
  self.clipsToBounds = NO;

  self.layer.shadowColor = [UIColor blackColor].CGColor;
  self.layer.shadowOffset = kAssistantShadowOffset;
  self.layer.shadowRadius = kAssistantShadowRadius;
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
  // Set on the content view to avoid being overridden by embedders.
  _contentView.accessibilityIdentifier =
      kAssistantContainerAccessibilityIdentifier;
  [_bottomRoundingView addSubview:_contentView];

  _grabberButton = [self createGrabberButton];
  [self addSubview:_grabberButton];

  [NSLayoutConstraint activateConstraints:@[
    // Grabber button constraints.
    [_grabberButton.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [_grabberButton.topAnchor constraintEqualToAnchor:self.topAnchor
                                             constant:kGrabberTopMargin],
    [_grabberButton.widthAnchor constraintEqualToConstant:kGrabberWidth],
    [_grabberButton.heightAnchor constraintEqualToConstant:kGrabberHeight],
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

// Creates and configures the grabber button.
- (AssistantGrabberButton*)createGrabberButton {
  AssistantGrabberButton* grabberButton =
      [AssistantGrabberButton buttonWithType:UIButtonTypeCustom];
  grabberButton.translatesAutoresizingMaskIntoConstraints = NO;
  grabberButton.backgroundColor = [UIColor colorNamed:kTertiaryBackgroundColor];
  grabberButton.layer.cornerRadius = kGrabberHeight / 2.0;

  grabberButton.isAccessibilityElement = YES;
  grabberButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_ASSISTANT_SHEET_GRABBER_ACCESSIBILITY_LABEL);
  grabberButton.accessibilityHint = l10n_util::GetNSString(
      IDS_IOS_ASSISTANT_SHEET_GRABBER_ACCESSIBILITY_HINT);
  grabberButton.accessibilityTraits =
      UIAccessibilityTraitAdjustable | UIAccessibilityTraitButton;

  return grabberButton;
}

#pragma mark - UIAccessibility

- (NSArray*)accessibilityElements {
  NSMutableArray* elements = [[NSMutableArray alloc] init];
  if (_grabberButton && !_grabberButton.isHidden) {
    [elements addObject:_grabberButton];
  }
  if (_contentView) {
    [elements addObject:_contentView];
  }
  return elements;
}

@end
