// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/interactive_lens_overlay_promo_view_controller.h"

#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Margins for the Lens view.
const CGFloat kLensViewTopMargin = 40.0;
const CGFloat kLensViewHorizontalMargin = 20.0;
// Height multipliers for the Lens view.
const CGFloat kLensViewMinHeightMultiplier = 0.4;
const CGFloat kLensViewMaxHeightMultiplier = 1.45;
// Corner radius for the top two corners of the Lens view.
const CGFloat kLensViewCornerRadius = 30.0;
// Top margin for tip bubble.
const CGFloat kBubbleViewTopMargin = 10.0;
// Height constant for the bubble view.
const CGFloat kBubbleViewHeightConstant = 70.0;
// Top margin for scroll view.
const CGFloat kScrollViewTopMargin = 45.0;
// Minimum height for the footer view.
const CGFloat kMinFooterHeight = 100.0;
}  // namespace

@implementation InteractiveLensOverlayPromoViewController {
  // View for the tip bubble.
  BubbleView* _bubbleView;
  // View for the interactive Lens instance.
  UIView* _lensView;
  // Scroll view containing the screen's title and subtitle.
  UIScrollView* _textScrollView;
}

- (instancetype)initWithLensView:(UIView*)lensView {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _lensView = lensView;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* view = self.view;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  UILayoutGuide* widthLayoutGuide = AddPromoStyleWidthLayoutGuide(view);

  // Add a gradient to the background.
  GradientView* gradientView = [[GradientView alloc]
      initWithTopColor:[UIColor colorNamed:kPrimaryBackgroundColor]
           bottomColor:[UIColor colorNamed:kSecondaryBackgroundColor]];
  gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:gradientView];
  AddSameConstraints(gradientView, view);

  // Create and constrain the scroll view containing the title and subtitle. The
  // content will only be scrollable after the Lens view has first compressed as
  // much as it can.
  _textScrollView = [self textScrollView];
  [view addSubview:_textScrollView];
  [NSLayoutConstraint activateConstraints:@[
    [_textScrollView.topAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.topAnchor
                       constant:kScrollViewTopMargin],
    [_textScrollView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor],
    [_textScrollView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor],
  ]];
  NSLayoutConstraint* heightConstraint = [_textScrollView.heightAnchor
      constraintEqualToAnchor:_textScrollView.contentLayoutGuide.heightAnchor];
  // UILayoutPriorityDefaultHigh is the default priority for content
  // compression. Setting this lower avoids compressing the content of the
  // scroll view.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;

  // Create and constrain the footer view containing the action button.
  UIView* footerContainerView = [self footerContainerView];
  [view addSubview:footerContainerView];
  AddSameConstraintsToSides(
      footerContainerView, view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
  [NSLayoutConstraint activateConstraints:@[
    [footerContainerView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kMinFooterHeight],
  ]];

  // Add and constrain the Lens view.
  [view addSubview:_lensView];
  _lensView.translatesAutoresizingMaskIntoConstraints = NO;
  _lensView.layer.cornerRadius = kLensViewCornerRadius;
  _lensView.layer.masksToBounds = YES;
  _lensView.layer.maskedCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
  [NSLayoutConstraint activateConstraints:@[
    // The Lens view has both a minimum possible height (relative to the height
    // of its superview) and a maximum possible height (relative to the height
    // of the Lens overlay image asset).
    [_lensView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:view.heightAnchor
                                  multiplier:kLensViewMinHeightMultiplier],
    [_lensView.heightAnchor
        constraintLessThanOrEqualToAnchor:_lensView.widthAnchor
                               multiplier:kLensViewMaxHeightMultiplier],
    [_lensView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor
                       constant:kLensViewHorizontalMargin],
    [_lensView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor
                       constant:-kLensViewHorizontalMargin],
    [_lensView.bottomAnchor
        constraintEqualToAnchor:footerContainerView.topAnchor],
    [_lensView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_textScrollView.bottomAnchor
                                    constant:kLensViewTopMargin],
  ]];

  // Create and constrain the bubble view so that it is pinned to the Lens view
  // and always below the title/subtitle.
  _bubbleView = [self bubbleView];
  [view addSubview:_bubbleView];
  [NSLayoutConstraint activateConstraints:@[
    [_bubbleView.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],
    [_bubbleView.leadingAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.leadingAnchor],
    [_bubbleView.trailingAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.trailingAnchor],
    [_bubbleView.bottomAnchor
        constraintEqualToAnchor:_lensView.topAnchor
                       constant:kBubbleViewHeightConstant],
    [_bubbleView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_textScrollView.bottomAnchor
                                    constant:kBubbleViewTopMargin],
  ]];

  [self startBubbleAnimation];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_textScrollView flashScrollIndicators];
}

#pragma mark - Private

// Creates and returns a scroll view containing the title and subtitle texts.
- (UIScrollView*)textScrollView {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.numberOfLines = 0;
  titleLabel.font = GetFRETitleFont(GetTitleLabelFontTextStyle(self));
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_INTERACTIVE_LENS_OVERLAY_PROMO_TITLE);
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;

  UILabel* subtitleLabel = [[UILabel alloc] init];
  subtitleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.textColor = [UIColor colorNamed:kGrey800Color];
  subtitleLabel.text =
      l10n_util::GetNSString(IDS_IOS_INTERACTIVE_LENS_OVERLAY_PROMO_SUBTITLE);
  subtitleLabel.textAlignment = NSTextAlignmentCenter;
  subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  subtitleLabel.adjustsFontForContentSizeCategory = YES;

  UIStackView* textStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, subtitleLabel ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.alignment = UIStackViewAlignmentCenter;
  textStack.spacing = 10.0;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.showsVerticalScrollIndicator = YES;
  [scrollView addSubview:textStack];

  [NSLayoutConstraint activateConstraints:@[
    [textStack.topAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor],
    [textStack.bottomAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor],
    [textStack.widthAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.widthAnchor],
    [textStack.centerXAnchor constraintEqualToAnchor:scrollView.centerXAnchor],
  ]];

  return scrollView;
}

// Creates and returns the tip bubble view.
- (BubbleView*)bubbleView {
  BubbleView* bubbleView = [[BubbleView alloc]
        initWithText:l10n_util::GetNSString(
                         IDS_IOS_INTERACTIVE_LENS_OVERLAY_PROMO_BUBBLE_TEXT)
      arrowDirection:BubbleArrowDirectionDown
           alignment:BubbleAlignmentCenter];
  bubbleView.translatesAutoresizingMaskIntoConstraints = NO;

  return bubbleView;
}

// Starts the animation for the tip bubble view.
- (void)startBubbleAnimation {
  CGFloat originalY = _bubbleView.frame.origin.y;
  CGFloat floatHeight = 18.0;

  __weak __typeof(_bubbleView) weakBubbleView = _bubbleView;
  [UIView animateWithDuration:1.5
                        delay:0.0
                      options:UIViewAnimationOptionAutoreverse |
                              UIViewAnimationOptionRepeat |
                              UIViewAnimationOptionCurveEaseInOut
                   animations:^{
                     CGRect frame = weakBubbleView.frame;
                     frame.origin.y = originalY - floatHeight;
                     weakBubbleView.frame = frame;
                   }
                   completion:nil];
}

// Creates and returns the footer container view, which holds the action button
// and a separator line.
- (UIView*)footerContainerView {
  UIView* footerContainerView = [[UIView alloc] init];
  footerContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  footerContainerView.backgroundColor = [UIColor whiteColor];

  UIView* separatorLine = [[UIView alloc] init];
  separatorLine.translatesAutoresizingMaskIntoConstraints = NO;
  separatorLine.backgroundColor = [UIColor colorWithWhite:0.9 alpha:1.0];
  [footerContainerView addSubview:separatorLine];

  [NSLayoutConstraint activateConstraints:@[
    [separatorLine.topAnchor
        constraintEqualToAnchor:footerContainerView.topAnchor],
    [separatorLine.leadingAnchor
        constraintEqualToAnchor:footerContainerView.leadingAnchor],
    [separatorLine.trailingAnchor
        constraintEqualToAnchor:footerContainerView.trailingAnchor],
    [separatorLine.heightAnchor constraintEqualToConstant:1.0]
  ]];

  UIButton* button = [self buttonView];
  [footerContainerView addSubview:button];

  [NSLayoutConstraint activateConstraints:@[
    [button.centerXAnchor
        constraintEqualToAnchor:footerContainerView.centerXAnchor],
    [button.centerYAnchor
        constraintEqualToAnchor:footerContainerView.centerYAnchor
                       constant:-5],
  ]];

  return footerContainerView;
}

// Creates and returns the action button.
- (UIButton*)buttonView {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  NSString* buttonTitle = l10n_util::GetNSString(
      IDS_IOS_INTERACTIVE_LENS_OVERLAY_PROMO_SKIP_BUTTON);
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.title = buttonTitle;
  buttonConfiguration.background.backgroundColor = [UIColor clearColor];
  buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* string =
      [[NSMutableAttributedString alloc] initWithString:buttonTitle];
  [string addAttributes:attributes range:NSMakeRange(0, string.length)];
  buttonConfiguration.attributedTitle = string;
  buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;
  button.configuration = buttonConfiguration;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();
  [button addTarget:self
                action:@selector(buttonTapped)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Handles taps on the action button.
- (void)buttonTapped {
  [self.delegate didTapContinueButton];
}

@end
