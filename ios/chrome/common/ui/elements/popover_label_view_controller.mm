// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"

namespace {

// Vertical inset for the text content.
constexpr CGFloat kVerticalInsetValue = 16;
// Horizontal inset for the text content.
constexpr CGFloat kHorizontalInsetValue = 16;
// Desired percentage of the width of the presented view controller.
constexpr CGFloat kWidthProportion = 0.75;
// Max width for the popover.
constexpr CGFloat kMaxWidth = 300;
// Distance between the primary text label and the secondary text label.
constexpr CGFloat kVerticalDistance = 10;
// Distance between the icon and the first letter in the secondary text box.
constexpr CGFloat kIconDistance = 10;
// The size of the icon at the left of the secondary text.
constexpr CGFloat kIconSize = 16;

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

// The image of the icon at the left of the secondary text. No icon if left
// nil.
@property(nonatomic, strong, readonly) UIImage* icon;

// Visual effect view used to add a blur effect to the popover.
@property(nonatomic, strong) UIVisualEffectView* blurBackgroundView;

@end

@implementation PopoverLabelViewController

- (instancetype)initWithMessage:(NSString*)message {
  NSDictionary* generalAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
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
  self = [self initWithPrimaryAttributedString:primaryAttributedString
                     secondaryAttributedString:secondaryAttributedString
                                          icon:nil];
  return self;
}

- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString
                                           icon:(UIImage*)icon {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _primaryAttributedString = primaryAttributedString;
    _secondaryAttributedString = secondaryAttributedString;
    _icon = icon;
    self.modalPresentationStyle = UIModalPresentationPopover;
    self.popoverPresentationController.delegate = self;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = UIColor.clearColor;

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.backgroundColor = UIColor.clearColor;
  _scrollView.delaysContentTouches = NO;
  _scrollView.showsVerticalScrollIndicator = YES;
  _scrollView.showsHorizontalScrollIndicator = NO;
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_scrollView];

  AddSameConstraints(self.view.safeAreaLayoutGuide, _scrollView);

  // TODO(crbug.com/40138105): Remove the following workaround:
  // Using a UIView instead of UILayoutGuide as the later behaves weirdly with
  // the scroll view.
  UIView* textContainerView = [[UIView alloc] init];
  textContainerView.backgroundColor = UIColor.clearColor;
  textContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:textContainerView];
  AddSameConstraints(textContainerView, _scrollView);

  UITextView* textView = CreateUITextViewWithTextKit1();
  textView.scrollEnabled = NO;
  textView.editable = NO;
  textView.delegate = self;
  textView.backgroundColor = [UIColor clearColor];
  textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  textView.adjustsFontForContentSizeCategory = YES;
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  textView.textContainerInset = UIEdgeInsetsZero;
  textView.textColor = [UIColor colorNamed:kTextSecondaryColor];
  textView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};

  if (self.primaryAttributedString) {
    textView.attributedText = self.primaryAttributedString;
  }

  [_scrollView addSubview:textView];

  // Only create secondary TextView when `secondaryAttributedString` is not nil
  // or empty. Set the constraint accordingly.
  if (self.secondaryAttributedString.length) {
    UITextView* secondaryTextView = CreateUITextViewWithTextKit1();
    secondaryTextView.scrollEnabled = NO;
    secondaryTextView.editable = NO;
    secondaryTextView.delegate = self;
    secondaryTextView.backgroundColor = [UIColor clearColor];
    secondaryTextView.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    secondaryTextView.adjustsFontForContentSizeCategory = YES;
    secondaryTextView.translatesAutoresizingMaskIntoConstraints = NO;
    secondaryTextView.textContainerInset = UIEdgeInsetsZero;
    secondaryTextView.textColor = [UIColor colorNamed:kTextSecondaryColor];
    secondaryTextView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    secondaryTextView.attributedText = self.secondaryAttributedString;

    [_scrollView addSubview:secondaryTextView];

    if (self.icon) {
      // Add an icon at the left of the secondary text box.

      UIImageView* imageView = [[UIImageView alloc] initWithImage:self.icon];
      imageView.translatesAutoresizingMaskIntoConstraints = NO;
      imageView.contentMode = UIViewContentModeScaleAspectFit;
      imageView.clipsToBounds = YES;

      [_scrollView addSubview:imageView];

      const CGFloat lineFragmentPadding =
          secondaryTextView.textContainer.lineFragmentPadding;

      [NSLayoutConstraint activateConstraints:@[
        [textContainerView.leadingAnchor
            constraintEqualToAnchor:imageView.leadingAnchor
                           constant:-kHorizontalInsetValue -
                                    lineFragmentPadding],
        [secondaryTextView.leadingAnchor
            constraintEqualToAnchor:imageView.trailingAnchor
                           constant:kIconDistance - lineFragmentPadding],
        [textView.bottomAnchor constraintEqualToAnchor:imageView.topAnchor
                                              constant:-kVerticalDistance],
        [imageView.heightAnchor constraintEqualToConstant:kIconSize],
        [imageView.widthAnchor constraintEqualToConstant:kIconSize],

      ]];
    } else {
      // Set default secondary text constraints when there is no icon.
      [NSLayoutConstraint activateConstraints:@[
        [textContainerView.leadingAnchor
            constraintEqualToAnchor:secondaryTextView.leadingAnchor
                           constant:-kHorizontalInsetValue],

      ]];
    }

    [NSLayoutConstraint activateConstraints:@[
      [textContainerView.widthAnchor
          constraintEqualToAnchor:_scrollView.widthAnchor],
      [textContainerView.leadingAnchor
          constraintEqualToAnchor:textView.leadingAnchor
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

  [self updateBackgroundColor];
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

  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    [self updateBackgroundColor];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updatePreferredContentSize];
}

#pragma mark - UIPopoverPresentationControllerDelegate

- (void)popoverPresentationController:
            (UIPopoverPresentationController*)popoverPresentationController
          willRepositionPopoverToRect:(inout CGRect*)rect
                               inView:(inout UIView**)view {
  // Popover moved to a different location, there might be more space available
  // now so a new layout pass is needed.
  [self.view setNeedsLayout];
}

#pragma mark - Private methods

- (void)updateBackgroundColor {
  // The popover background in dark mode needs more contrast.
  BOOL darkMode = UITraitCollection.currentTraitCollection.userInterfaceStyle ==
                  UIUserInterfaceStyleDark;

  self.view.backgroundColor =
      darkMode ? [UIColor colorNamed:kTertiaryBackgroundColor]
               : UIColor.clearColor;

  if (darkMode && self.blurBackgroundView.superview) {
    // Remove blurred background in dark mode if it has been added.
    [self.blurBackgroundView removeFromSuperview];
  } else if (!darkMode && !self.blurBackgroundView.superview) {
    // Add blurred background in light mode only if it has not been added
    // already.
    [self.view insertSubview:self.blurBackgroundView atIndex:0];
  }
}

#pragma mark - Properties

- (UIVisualEffectView*)blurBackgroundView {
  if (!_blurBackgroundView) {
    // Set up a blurred background.
    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterial];
    _blurBackgroundView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    _blurBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    _blurBackgroundView.frame = self.view.bounds;
  }
  return _blurBackgroundView;
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
  // Expected width of the `self.scrollView`.
  CGFloat width =
      self.presentingViewController.view.bounds.size.width * kWidthProportion;
  // Cap max width at 300pt.
  if (width > kMaxWidth) {
    width = kMaxWidth;
  }
  // `scrollView` is used here instead of `self.view`, because `self.view`
  // includes arrow size during calculation although it's being added to the
  // result size anyway.
  CGSize size =
      [self.scrollView systemLayoutSizeFittingSize:CGSizeMake(width, 0)
                     withHorizontalFittingPriority:UILayoutPriorityRequired
                           verticalFittingPriority:500];
  [UIView performWithoutAnimation:^{
    self.preferredContentSize = size;
  }];
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
