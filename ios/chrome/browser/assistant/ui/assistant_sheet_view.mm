// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_sheet_view.h"

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

// Header styling.
constexpr CGFloat kTitleVerticalMargin = 12.0;

// Returns the background color for the sheet.
constexpr CGFloat kSheetBackgroundAlpha = 0.5;
UIColor* SheetBackgroundColor() {
  return [[UIColor colorNamed:kSecondaryBackgroundColor]
      colorWithAlphaComponent:kSheetBackgroundAlpha];
}

}  // namespace

@implementation AssistantSheetView {
  UIScrollView* _scrollView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self configureSheetStyling];
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
}

- (CGFloat)preferredHeight {
  // Force layout to ensure subviews are sized correctly.
  [self layoutIfNeeded];

  CGFloat headerHeight =
      [_headerView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;

  // Calculate content height based on the width of the view.
  CGSize targetSize =
      CGSizeMake(self.bounds.size.width, UILayoutFittingCompressedSize.height);
  CGFloat contentHeight =
      [_contentView
            systemLayoutSizeFittingSize:targetSize
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;

  return headerHeight + contentHeight;
}

#pragma mark - Private

// Configures the visual styling of the sheet.
// TODO(crbug.com/390204874): Update the sheet styling to perfectly match the
// design.
- (void)configureSheetStyling {
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
  tintView.backgroundColor = SheetBackgroundColor();
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
  _headerView = [self createHeaderView];
  [self addSubview:_headerView];

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_scrollView];

  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_contentView];

  // Add a low-priority height constraint to allow content to fill the scroll
  // view if it's smaller, while still allowing it to grow larger.
  NSLayoutConstraint* contentViewHeightConstraint = [_contentView.heightAnchor
      constraintGreaterThanOrEqualToAnchor:_scrollView.heightAnchor];
  contentViewHeightConstraint.priority = UILayoutPriorityDefaultLow;

  [NSLayoutConstraint activateConstraints:@[
    // Header view constraints.
    [_headerView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_headerView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_headerView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],

    // Scroll view constraints.
    [_scrollView.topAnchor constraintEqualToAnchor:_headerView.bottomAnchor],
    [_scrollView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_scrollView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [_scrollView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],

    // Content view constraints.
    [_contentView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_contentView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [_contentView.topAnchor constraintEqualToAnchor:_scrollView.topAnchor],
    [_contentView.bottomAnchor
        constraintEqualToAnchor:_scrollView.bottomAnchor],
    contentViewHeightConstraint,
  ]];
}

// Creates the header view with Grabber.
- (UIView*)createHeaderView {
  UIView* headerView = [[UIView alloc] init];
  headerView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* grabberView = [self createGrabberView];
  [headerView addSubview:grabberView];

  [NSLayoutConstraint activateConstraints:@[
    // Grabber.
    [grabberView.topAnchor constraintEqualToAnchor:headerView.topAnchor
                                          constant:kGrabberTopMargin],
    [grabberView.bottomAnchor constraintEqualToAnchor:headerView.bottomAnchor
                                             constant:-kTitleVerticalMargin],
    [grabberView.centerXAnchor
        constraintEqualToAnchor:headerView.centerXAnchor],
    [grabberView.widthAnchor constraintEqualToConstant:kGrabberWidth],
    [grabberView.heightAnchor constraintEqualToConstant:kGrabberHeight],
  ]];

  return headerView;
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
