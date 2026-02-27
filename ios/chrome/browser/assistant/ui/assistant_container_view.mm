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

// Corner styling.
const CGFloat kCornerRadius = 22.0;

// Grabber styling.
constexpr CGFloat kGrabberWidth = 33.0;
constexpr CGFloat kGrabberHeight = 4.0;
constexpr CGFloat kGrabberTopMargin = 5.0;
constexpr CGFloat kGrabberAlpha = 0.24;

// Content styling.
constexpr CGFloat kContentTopMargin = 16.0;

// Returns the background color for the container.
constexpr CGFloat kContainerBackgroundAlpha = 0.5;
UIColor* ContainerBackgroundColor() {
  return [[UIColor colorNamed:kSecondaryBackgroundColor]
      colorWithAlphaComponent:kContainerBackgroundAlpha];
}

}  // namespace

@implementation AssistantContainerView {
  UIView* _scrollContainerView;
  UIScrollView* _scrollView;
  UIView* _grabberView;
  CAGradientLayer* _maskLayer;
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
  // Update shadow path to match the view's bounds and corner radius.
  self.layer.shadowPath = [UIBezierPath bezierPathWithRoundedRect:self.bounds
                                                     cornerRadius:kCornerRadius]
                              .CGPath;

  [self updateScrollContainerMask];
}

- (NSInteger)preferredHeight {
  CGSize targetSize =
      CGSizeMake(self.bounds.size.width, UILayoutFittingCompressedSize.height);
  CGFloat contentHeight =
      [_contentView
            systemLayoutSizeFittingSize:targetSize
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;
  return round(kContentTopMargin + contentHeight);
}

#pragma mark - Private

// Configures the visual styling of the container.
// TODO(crbug.com/390204874): Update the container styling to perfectly match
// the design.
- (void)configureContainerStyling {
  // Container for visual effects (Blur + Tint) that clips to corners.
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
  UIVisualEffectView* blurView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  blurView.translatesAutoresizingMaskIntoConstraints = NO;
  blurView.layer.cornerRadius = kCornerRadius;
  blurView.clipsToBounds = YES;
  [self insertSubview:blurView atIndex:0];

  AddSameConstraints(blurView, self);

  // Tint view overlaying the blur effect to provide the background color.
  UIView* tintView = [[UIView alloc] init];
  tintView.translatesAutoresizingMaskIntoConstraints = NO;
  tintView.backgroundColor = ContainerBackgroundColor();
  [blurView.contentView
      addSubview:tintView];  // Add to visual effect contentView.

  AddSameConstraints(tintView, blurView);

  self.layer.shadowColor = [UIColor blackColor].CGColor;
  self.layer.shadowOpacity = kShadowOpacity;
  self.layer.shadowOffset = kShadowOffset;
  self.layer.shadowRadius = kShadowRadius;
}

// Sets up the view hierarchy by creating and adding subviews.
- (void)setUpSubviews {
  _scrollContainerView = [[UIView alloc] init];
  _scrollContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_scrollContainerView];

  // Initialize the gradient mask layer to fade out content at the top.
  _maskLayer = [CAGradientLayer layer];
  _maskLayer.colors = @[
    (id)[UIColor clearColor].CGColor, (id)[UIColor blackColor].CGColor,
    (id)[UIColor blackColor].CGColor
  ];
  _scrollContainerView.layer.mask = _maskLayer;

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollContainerView addSubview:_scrollView];

  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_contentView];

  _grabberView = [self createGrabberView];
  [self addSubview:_grabberView];

  // Add a low-priority height constraint to allow content to fill the scroll
  // view if it's smaller, while still allowing it to grow larger.
  NSLayoutConstraint* contentViewHeightConstraint = [_contentView.heightAnchor
      constraintGreaterThanOrEqualToAnchor:_scrollView.heightAnchor];
  contentViewHeightConstraint.priority = UILayoutPriorityDefaultLow;

  [NSLayoutConstraint activateConstraints:@[
    // Grabber view constraints.
    [_grabberView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [_grabberView.topAnchor constraintEqualToAnchor:self.topAnchor
                                           constant:kGrabberTopMargin],
    [_grabberView.widthAnchor constraintEqualToConstant:kGrabberWidth],
    [_grabberView.heightAnchor constraintEqualToConstant:kGrabberHeight],

    // Content view constraints.
    [_contentView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_contentView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [_contentView.topAnchor constraintEqualToAnchor:_scrollView.topAnchor],
    [_contentView.bottomAnchor
        constraintEqualToAnchor:_scrollView.bottomAnchor],
    contentViewHeightConstraint,
  ]];

  AddSameConstraints(_scrollContainerView, self);
  AddSameConstraints(_scrollContainerView, _scrollView);
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

// Updates the gradient mask over the scroll container to ensure
// exactly the top `kContentTopMargin` is faded out.
- (void)updateScrollContainerMask {
  _maskLayer.frame = _scrollContainerView.bounds;

  CGFloat containerHeight = _scrollContainerView.bounds.size.height;
  if (containerHeight <= 0) {
    return;
  }

  // To ensure exactly `kContentTopMargin` points are faded out the ratio must
  // be calculated dynamically.
  CGFloat fadeRatio = MIN(1.0, kContentTopMargin / containerHeight);
  _maskLayer.locations = @[ @0.0, @(fadeRatio), @1.0 ];
}

@end
