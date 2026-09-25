// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_empty_view.h"

#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Spacing between the image and the message in the zero-state empty view.
constexpr CGFloat kEmptyViewSpacing = 16.0;

// Horizontal margin for the zero-state message label.
constexpr CGFloat kEmptyViewHorizontalMargin = 16.0;

// Bottom margin below the zero-state message label.
constexpr CGFloat kEmptyViewBottomMargin = 8.0;

// Maximum width of the zero-state message label.
constexpr CGFloat kEmptyViewMessageMaxWidth = 300.0;

// Upward vertical offset for the zero-state image view center.
constexpr CGFloat kEmptyViewImageCenterYOffset = -45.0;

}  // namespace

@implementation AtMemoryEmptyView {
  UIScrollView* _scrollView;
  UIView* _contentView;
  UIImageView* _imageView;
  UILabel* _messageLabel;
}

- (instancetype)initWithFrame:(CGRect)frame
                        image:(UIImage*)image
                      message:(NSString*)message {
  self = [super initWithFrame:frame];
  if (self) {
    // Expose the empty state as a single element so VoiceOver reads the
    // message directly rather than having to descend into the scroll view.
    // The image is decorative and gets no separate focus stop.
    self.isAccessibilityElement = YES;
    self.accessibilityTraits = UIAccessibilityTraitStaticText;
    self.accessibilityIdentifier = kAtMemoryEmptyViewAccessibilityIdentifier;

    [self setUpSubviewsWithImage:image];
    [self setupConstraints];
    [self updateMessage:message];
  }
  return self;
}

#pragma mark - Public

- (void)updateMessage:(NSString*)message {
  _messageLabel.text = message;
  // Route through the property setter so `viewAccessibilityLabel` remains the
  // only writer of the label's accessibility label.
  self.viewAccessibilityLabel = message;
}

- (NSString*)viewAccessibilityLabel {
  return self.accessibilityLabel;
}

- (void)setViewAccessibilityLabel:(NSString*)label {
  // Skip redundant writes: reassigning the label of an on-screen element makes
  // VoiceOver re-announce it and can drop the user's reading position.
  if (self.accessibilityLabel == label ||
      [self.accessibilityLabel isEqualToString:label]) {
    return;
  }
  self.accessibilityLabel = label;
}

#pragma mark - Private

// Creates the scroll view, content view, image view, and message label.
- (void)setUpSubviewsWithImage:(UIImage*)image {
  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_scrollView];

  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_contentView];

  _imageView = [[UIImageView alloc] initWithImage:image];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [_contentView addSubview:_imageView];

  _messageLabel = [[UILabel alloc] init];
  _messageLabel.numberOfLines = 0;
  _messageLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _messageLabel.adjustsFontForContentSizeCategory = YES;
  _messageLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _messageLabel.textAlignment = NSTextAlignmentCenter;
  _messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [_contentView addSubview:_messageLabel];
}

// Activates layout constraints for `_scrollView`, `_contentView`, `_imageView`,
// and `_messageLabel`.
- (void)setupConstraints {
  AddSameConstraintsToSides(_scrollView, self.safeAreaLayoutGuide,
                            LayoutSides::kTop | LayoutSides::kHorizontal);
  AddSameConstraints(_contentView, _scrollView);

  // Match the visible height unless the label requires more space, in which
  // case the content grows and becomes scrollable.
  NSLayoutConstraint* contentHeightConstraint = [_contentView.heightAnchor
      constraintEqualToAnchor:_scrollView.heightAnchor];
  contentHeightConstraint.priority = UILayoutPriorityDefaultLow;

  [NSLayoutConstraint activateConstraints:@[
    // Pin the bottom to the keyboard layout guide so the content recenters in
    // the visible area when the keyboard is shown.
    [_scrollView.bottomAnchor
        constraintEqualToAnchor:self.keyboardLayoutGuide.topAnchor],
    [_contentView.widthAnchor constraintEqualToAnchor:_scrollView.widthAnchor],
    contentHeightConstraint,

    // Keep the image centered horizontally and vertically with an offset.
    [_imageView.centerXAnchor
        constraintEqualToAnchor:_contentView.centerXAnchor],
    [_imageView.centerYAnchor
        constraintEqualToAnchor:_contentView.centerYAnchor
                       constant:kEmptyViewImageCenterYOffset],
    [_imageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_contentView.topAnchor],

    // Top-align message label directly under the image with a defined gap.
    [_messageLabel.topAnchor constraintEqualToAnchor:_imageView.bottomAnchor
                                            constant:kEmptyViewSpacing],
    [_messageLabel.centerXAnchor
        constraintEqualToAnchor:_contentView.centerXAnchor],
    [_messageLabel.widthAnchor
        constraintLessThanOrEqualToConstant:kEmptyViewMessageMaxWidth],
    [_messageLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_contentView.leadingAnchor
                                    constant:kEmptyViewHorizontalMargin],
    [_messageLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_contentView.trailingAnchor
                                 constant:-kEmptyViewHorizontalMargin],

    // The bottom constraint determines the content height from the label lines.
    [_contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:_messageLabel.bottomAnchor
                                    constant:kEmptyViewBottomMargin],
  ]];
}

@end
