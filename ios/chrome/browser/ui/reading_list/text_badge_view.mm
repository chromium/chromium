// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/text_badge_view.h"

#import "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
const CGFloat kFontSize = 11.0f;
// The margin between the top and bottom of the label and the badge.
const CGFloat kLabelVerticalMargin = 2.5f;
// The default value for the margin between the sides of the label and the
// badge.
const CGFloat kDefaultLabelHorizontalMargin = 8.5f;
}

@interface TextBadgeView ()
// Label containing the text displayed on the badge.
@property(nonatomic, strong) UILabel* label;
// The margin between the sides of the label and the badge.
@property(nonatomic, assign, readonly) CGFloat labelHorizontalMargin;
// Indicate whether `label` has been added as a subview of the TextBadgeView.
@property(nonatomic, assign) BOOL didAddSubviews;
@end

@implementation TextBadgeView

@synthesize label = _label;
@synthesize labelHorizontalMargin = _labelHorizontalMargin;
@synthesize didAddSubviews = _didAddSubviews;

- (instancetype)initWithText:(NSString*)text
       labelHorizontalMargin:(CGFloat)margin {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _label = [TextBadgeView labelWithText:text];
    _labelHorizontalMargin = margin;
    _didAddSubviews = NO;
  }
  return self;
}

- (instancetype)initWithText:(NSString*)text {
  return [self initWithText:text
      labelHorizontalMargin:kDefaultLabelHorizontalMargin];
}

#pragma mark - UIView overrides

// Override `willMoveToSuperview` to add view properties to the view hierarchy
// and set the badge's appearance.
- (void)willMoveToSuperview:(UIView*)newSuperview {
  if (!self.didAddSubviews) {
    [self addSubview:self.label];
    self.didAddSubviews = YES;
    [self activateConstraints];
    [self setBackgroundColor:[UIColor colorNamed:kBlueHaloColor]];
    [self setAccessibilityLabel:self.label.text];
    self.isAccessibilityElement = YES;
  }
  [super willMoveToSuperview:newSuperview];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  // Set the badge's corner radius to be one half of its height. This causes the
  // ends of the badge to be circular.
  self.layer.cornerRadius = self.bounds.size.height / 2.0f;
}

#pragma mark - Public properties

- (NSString*)text {
  return self.label.text;
}

- (void)setText:(NSString*)text {
  DCHECK(text.length);
  self.label.text = text;
}

#pragma mark - Private class methods

// Return a label that displays text in white with center alignment.
+ (UILabel*)labelWithText:(NSString*)text {
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  [label
      setFont:[UIFont systemFontOfSize:kFontSize weight:UIFontWeightSemibold]];
  [label setTextColor:[UIColor colorNamed:kBlueColor]];
  [label setTranslatesAutoresizingMaskIntoConstraints:NO];
  [label setText:text];
  [label setTextAlignment:NSTextAlignmentCenter];
  return label;
}

#pragma mark - Private instance methods

// Activate constraints to properly position the badge and its subviews.
- (void)activateConstraints {
  // Make the badge width fit the label, adding a margin on both sides.
  NSLayoutConstraint* badgeWidthConstraint =
      [self.widthAnchor constraintEqualToAnchor:self.label.widthAnchor
                                       constant:self.labelHorizontalMargin * 2];
  // This constraint should not be satisfied if the label is taller than it is
  // wide, so make it optional.
  badgeWidthConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    // Center label on badge.
    [self.label.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [self.label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    // Make the badge height fit the label.
    [self.heightAnchor constraintEqualToAnchor:self.label.heightAnchor
                                      constant:kLabelVerticalMargin * 2],
    // Ensure that the badge will never be taller than it is wide. For
    // a tall label, the badge will look like a circle instead of an ellipse.
    [self.widthAnchor constraintGreaterThanOrEqualToAnchor:self.heightAnchor],
    badgeWidthConstraint
  ]];
}

@end
