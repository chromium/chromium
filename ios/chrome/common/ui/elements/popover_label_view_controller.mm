// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Vertical inset for the text content.
constexpr CGFloat kVerticalInsetValue = 20;
// Horizontal inset for the text content.
constexpr CGFloat kHorizontalInsetValue = 16;
// Desired percentage of the width of the presented view controller.
constexpr CGFloat kWidthProportion = 0.75;
// Max width for the popover.
constexpr CGFloat kMaxWidth = 300;
// Distance between the primary text label and the secondary text label.
constexpr CGFloat kVerticalDistance = 24;

}  // namespace

@interface PopoverLabelViewController () <
    UIPopoverPresentationControllerDelegate,
    UITextViewDelegate>

// UIScrollView which is used for size calculation.
@property(nonatomic, strong) UIScrollView* scrollView;

// The attributed string being presented as primary text.
@property(nonatomic, strong, readonly)
    NSAttributedString* primaryAttributedString;

// The attributed string being presented as secondary text.
@property(nonatomic, strong, readonly)
    NSAttributedString* secondaryAttributedString;

@end

@implementation PopoverLabelViewController

- (instancetype)initWithMessage:(NSString*)message {
  NSDictionary* generalAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor],
    NSFontAttributeName : [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
  };

  NSAttributedString* attributedString =
      [[NSAttributedString alloc] initWithString:message
                                      attributes:generalAttributes];
  return [self initWithPrimaryAttributedString:attributedString
                     secondaryAttributedString:nil];
}

- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _primaryAttributedString = primaryAttributedString;
    _secondaryAttributedString = secondaryAttributedString;
    self.modalPresentationStyle = UIModalPresentationPopover;
    self.popoverPresentationController.delegate = self;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.backgroundColor = UIColor.clearColor;
  _scrollView.delaysContentTouches = NO;
  _scrollView.showsVerticalScrollIndicator = YES;
  _scrollView.showsHorizontalScrollIndicator = NO;
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_scrollView];

  AddSameConstraints(self.view.safeAreaLayoutGuide, _scrollView);

  // TODO(crbug.com/1100884): Remove the following workaround:
  // Using a UIView instead of UILayoutGuide as the later behaves weirdly with
  // the scroll view.
  UIView* textContainerView = [[UIView alloc] init];
  textContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:textContainerView];
  AddSameConstraints(textContainerView, _scrollView);

  UITextView* textView = [[UITextView alloc] init];
  textView.scrollEnabled = NO;
  textView.editable = NO;
  textView.delegate = self;
  textView.backgroundColor = [UIColor clearColor];
  textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  textView.adjustsFontForContentSizeCategory = YES;
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  textView.textColor = [UIColor colorNamed:kTextSecondaryColor];
  textView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};

  if (self.primaryAttributedString) {
    textView.attributedText = self.primaryAttributedString;
  }

  [_scrollView addSubview:textView];

  // Only create secondary TextView when |secondaryAttributedString| is not nil
  // or empty. Set the constraint accordingly.
  if (self.secondaryAttributedString.length) {
    UITextView* secondaryTextView = [[UITextView alloc] init];
    secondaryTextView.scrollEnabled = NO;
    secondaryTextView.editable = NO;
    secondaryTextView.delegate = self;
    secondaryTextView.backgroundColor = [UIColor clearColor];
    secondaryTextView.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    secondaryTextView.adjustsFontForContentSizeCategory = YES;
    secondaryTextView.translatesAutoresizingMaskIntoConstraints = NO;
    secondaryTextView.textColor = [UIColor colorNamed:kTextSecondaryColor];
    secondaryTextView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    secondaryTextView.attributedText = self.secondaryAttributedString;

    [_scrollView addSubview:secondaryTextView];

    [NSLayoutConstraint activateConstraints:@[
      [textContainerView.widthAnchor
          constraintEqualToAnchor:_scrollView.widthAnchor],
      [textContainerView.leadingAnchor
          constraintEqualToAnchor:textView.leadingAnchor
                         constant:-kHorizontalInsetValue],
      [textContainerView.leadingAnchor
          constraintEqualToAnchor:secondaryTextView.leadingAnchor
                         constant:-kHorizontalInsetValue],
      [textContainerView.trailingAnchor
          constraintEqualToAnchor:textView.trailingAnchor
                         constant:kHorizontalInsetValue],
      [textContainerView.trailingAnchor
          constraintEqualToAnchor:secondaryTextView.trailingAnchor
                         constant:kHorizontalInsetValue],
      [textView.bottomAnchor constraintEqualToAnchor:secondaryTextView.topAnchor
                                            constant:-kVerticalDistance],
      [textContainerView.topAnchor
          constraintEqualToAnchor:textView.topAnchor
                         constant:-kVerticalInsetValue],
      [textContainerView.bottomAnchor
          constraintEqualToAnchor:secondaryTextView.bottomAnchor
                         constant:kVerticalInsetValue],
    ]];
  } else {
    // Constraints used when only have primary TextView.
    [NSLayoutConstraint activateConstraints:@[
      [textContainerView.widthAnchor
          constraintEqualToAnchor:_scrollView.widthAnchor],
      [textContainerView.leadingAnchor
          constraintEqualToAnchor:textView.leadingAnchor
                         constant:-kHorizontalInsetValue],
      [textContainerView.trailingAnchor
          constraintEqualToAnchor:textView.trailingAnchor
                         constant:kHorizontalInsetValue],
      [textContainerView.topAnchor
          constraintEqualToAnchor:textView.topAnchor
                         constant:-kVerticalInsetValue],
      [textContainerView.bottomAnchor
          constraintEqualToAnchor:textView.bottomAnchor
                         constant:kVerticalInsetValue],
    ]];
  }

  NSLayoutConstraint* heightConstraint = [_scrollView.heightAnchor
      constraintEqualToAnchor:_scrollView.contentLayoutGuide.heightAnchor];

  // UILayoutPriorityDefaultHigh is the default priority for content
  // compression. Setting this lower avoids compressing the content of the
  // scroll view.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;
}

- (void)viewWillAppear:(BOOL)animated {
  [self updatePreferredContentSize];
  [super viewWillAppear:animated];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if ((self.traitCollection.verticalSizeClass !=
       previousTraitCollection.verticalSizeClass) ||
      (self.traitCollection.horizontalSizeClass !=
       previousTraitCollection.horizontalSizeClass)) {
    [self updatePreferredContentSize];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (UIModalPresentationStyle)
    adaptivePresentationStyleForPresentationController:
        (UIPresentationController*)controller
                                       traitCollection:
                                           (UITraitCollection*)traitCollection {
  return UIModalPresentationNone;
}

#pragma mark - Helpers

// Updates the preferred content size according to the presenting view size and
// the layout size of the view.
- (void)updatePreferredContentSize {
  // Expected width of the |self.scrollView|.
  CGFloat width =
      self.presentingViewController.view.bounds.size.width * kWidthProportion;
  // Cap max width at 300pt.
  if (width > kMaxWidth) {
    width = kMaxWidth;
  }
  // |scrollView| is used here instead of |self.view|, because |self.view|
  // includes arrow size during calculation although it's being added to the
  // result size anyway.
  CGSize size =
      [self.scrollView systemLayoutSizeFittingSize:CGSizeMake(width, 0)
                     withHorizontalFittingPriority:UILayoutPriorityRequired
                           verticalFittingPriority:500];
  self.preferredContentSize = size;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if (URL) {
    [self.delegate didTapLinkURL:URL];
  }
  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

@end
