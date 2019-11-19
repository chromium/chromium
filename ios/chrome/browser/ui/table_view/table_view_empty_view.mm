// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_view_empty_view.h"

#import "ios/chrome/browser/ui/table_view/table_view_constants.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The StackView vertical spacing.
const float kStackViewVerticalSpacing = 23.0;
// The StackView width.
const float kStackViewWidth = 227.0;
// Returns |message| as an attributed string with default styling.
NSAttributedString* GetAttributedMessage(NSString* message) {
  NSMutableParagraphStyle* paragraph_style =
      [[NSMutableParagraphStyle alloc] init];
  paragraph_style.lineBreakMode = NSLineBreakByWordWrapping;
  paragraph_style.alignment = NSTextAlignmentCenter;
  NSDictionary* default_attributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSParagraphStyleAttributeName : paragraph_style
  };
  return [[NSAttributedString alloc] initWithString:message
                                         attributes:default_attributes];
}
}

@interface TableViewEmptyView ()
// The message that will be displayed and the label that will display it.
@property(nonatomic, copy) NSAttributedString* message;
@property(nonatomic, strong) UILabel* messageLabel;
// The image that will be displayed.
@property(nonatomic, strong) UIImage* image;
// The inner ScrollView so the whole content can be seen even if it is taller
// than the TableView.
@property(nonatomic, strong) UIScrollView* scrollView;
// The height constraint of the ScrollView.
@property(nonatomic, strong) NSLayoutConstraint* scrollViewHeight;
@end

@implementation TableViewEmptyView

- (instancetype)initWithFrame:(CGRect)frame
                      message:(NSString*)message
                        image:(UIImage*)image {
  if (self = [super initWithFrame:frame]) {
    _message = GetAttributedMessage(message);
    _image = image;
    self.accessibilityIdentifier = [[self class] accessibilityIdentifier];
  }
  return self;
}

- (instancetype)initWithFrame:(CGRect)frame
            attributedMessage:(NSAttributedString*)message
                        image:(UIImage*)image {
  if (self = [super initWithFrame:frame]) {
    _message = message;
    _image = image;
    self.accessibilityIdentifier = [[self class] accessibilityIdentifier];
  }
  return self;
}

#pragma mark - Accessors

- (NSString*)messageAccessibilityLabel {
  return self.messageLabel.accessibilityLabel;
}

- (void)setMessageAccessibilityLabel:(NSString*)label {
  if ([self.messageAccessibilityLabel isEqualToString:label])
    return;
  self.messageLabel.accessibilityLabel = label;
}

#pragma mark - Public

+ (NSString*)accessibilityIdentifier {
  return kTableViewEmptyViewID;
}

- (void)setScrollViewContentInsets:(UIEdgeInsets)scrollViewContentInsets {
  _scrollViewContentInsets = scrollViewContentInsets;
  self.scrollView.contentInset = scrollViewContentInsets;
  self.scrollViewHeight.constant =
      scrollViewContentInsets.top + scrollViewContentInsets.bottom;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - Private

- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0))
    return;

  // Scroll view used to scroll the content if it is too big.
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.contentInset = self.scrollViewContentInsets;
  self.scrollView = scrollView;

  UIImageView* imageView = [[UIImageView alloc] initWithImage:self.image];
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.clipsToBounds = YES;

  UILabel* messageLabel = [[UILabel alloc] init];
  messageLabel.numberOfLines = 0;
  messageLabel.attributedText = self.message;
  messageLabel.accessibilityLabel = self.message.string;
  self.messageLabel = messageLabel;

  // Vertical stack view that holds the image and message.
  UIStackView* verticalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ imageView, messageLabel ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = kStackViewVerticalSpacing;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.layoutMarginsRelativeArrangement = YES;
  verticalStack.layoutMargins = UIEdgeInsetsMake(kStackViewVerticalSpacing, 0,
                                                 kStackViewVerticalSpacing, 0);
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;

  [scrollView addSubview:verticalStack];
  [self addSubview:scrollView];

  // The scroll view should contains the stack view without scrolling enabled if
  // it is small enough.
  NSLayoutConstraint* scrollViewHeightConstraint = [scrollView.heightAnchor
      constraintEqualToAnchor:verticalStack.heightAnchor
                     constant:(self.scrollViewContentInsets.top +
                               self.scrollViewContentInsets.bottom)];
  scrollViewHeightConstraint.priority = UILayoutPriorityDefaultLow;
  scrollViewHeightConstraint.active = YES;
  self.scrollViewHeight = scrollViewHeightConstraint;

  [NSLayoutConstraint activateConstraints:@[
    // The vertical stack is horizontal centered.
    [verticalStack.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
    [verticalStack.bottomAnchor
        constraintEqualToAnchor:scrollView.bottomAnchor],
    [verticalStack.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [verticalStack.widthAnchor constraintEqualToConstant:kStackViewWidth],

    // Have the scroll view taking the full width of self and be vertically
    // centered, which is useful when the label isn't taking the full height.
    [scrollView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [scrollView.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor],
    [scrollView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [scrollView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ]];
}

@end
