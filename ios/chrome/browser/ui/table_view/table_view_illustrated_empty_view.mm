// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_view_illustrated_empty_view.h"

#import "ios/chrome/browser/ui/table_view/table_view_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The StackView vertical spacing between the image, the title and the subtitle.
const CGFloat kStackViewVerticalSpacingPt = 12.0;
// The StackView width.
const CGFloat kStackViewWidthPt = 310.0;
// The UIImageView height.
const CGFloat kImageHeightPt = 150.0;
}  // namespace

@interface TableViewIllustratedEmptyView ()
// The image that will be displayed.
@property(nonatomic, strong) UIImage* image;
// The title that will be displayed under the image.
@property(nonatomic, copy) NSString* title;
// The subtitle that will be displayed under the title.
@property(nonatomic, copy) NSString* subtitle;
// The inner ScrollView so the whole content can be seen even if it is taller
// than the TableView.
@property(nonatomic, strong) UIScrollView* scrollView;
// The height constraint of the ScrollView.
@property(nonatomic, strong) NSLayoutConstraint* scrollViewHeight;
@end

@implementation TableViewIllustratedEmptyView

// Synthesized from the ChromeEmptyTableViewBackground protocol
@synthesize scrollViewContentInsets = _scrollViewContentInsets;

- (instancetype)initWithFrame:(CGRect)frame
                        image:(UIImage*)image
                        title:(NSString*)title
                     subtitle:(NSString*)subtitle {
  if (self = [super initWithFrame:frame]) {
    _title = title;
    _subtitle = subtitle;
    _image = image;
    self.accessibilityIdentifier = [[self class] accessibilityIdentifier];
  }
  return self;
}

#pragma mark - Public

+ (NSString*)accessibilityIdentifier {
  return kTableViewIllustratedEmptyViewID;
}

#pragma mark - ChromeEmptyTableViewBackground

- (void)setScrollViewContentInsets:(UIEdgeInsets)scrollViewContentInsets {
  _scrollViewContentInsets = scrollViewContentInsets;
  self.scrollView.contentInset = scrollViewContentInsets;
  self.scrollViewHeight.constant =
      scrollViewContentInsets.top + scrollViewContentInsets.bottom;
}

- (NSString*)viewAccessibilityLabel {
  return self.accessibilityLabel;
}

- (void)setViewAccessibilityLabel:(NSString*)label {
  if ([self.viewAccessibilityLabel isEqualToString:label])
    return;
  self.accessibilityLabel = label;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - Private

// Create elements to display the image, title and subtitle. Add them to a
// StackView to arrange them. Then, add the StackView to a ScrollView to make
// the empty view scrollable if the content is taller than the frame.
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

  NSMutableArray* subviewsArray = [NSMutableArray arrayWithObject:imageView];

  if ([self.title length]) {
    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.isAccessibilityElement = NO;
    titleLabel.numberOfLines = 0;
    titleLabel.text = self.title;
    titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
    titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    titleLabel.textAlignment = NSTextAlignmentCenter;
    [subviewsArray addObject:titleLabel];
  }

  if ([self.subtitle length]) {
    UILabel* subtitleLabel = [[UILabel alloc] init];
    subtitleLabel.isAccessibilityElement = NO;
    subtitleLabel.numberOfLines = 0;
    subtitleLabel.text = self.subtitle;
    subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    subtitleLabel.textAlignment = NSTextAlignmentCenter;
    [subviewsArray addObject:subtitleLabel];
  }

  self.isAccessibilityElement = YES;
  self.accessibilityLabel = [NSString
      stringWithFormat:@"%@ - %@", self.title ?: @"", self.subtitle ?: @""];

  // Vertical stack view that holds the image, title and subtitle.
  UIStackView* verticalStack =
      [[UIStackView alloc] initWithArrangedSubviews:subviewsArray];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = kStackViewVerticalSpacingPt;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.layoutMarginsRelativeArrangement = YES;
  verticalStack.layoutMargins = UIEdgeInsetsMake(
      kStackViewVerticalSpacingPt, 0, kStackViewVerticalSpacingPt, 0);
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
    [verticalStack.widthAnchor constraintEqualToConstant:kStackViewWidthPt],
    [imageView.heightAnchor constraintEqualToConstant:kImageHeightPt],

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
