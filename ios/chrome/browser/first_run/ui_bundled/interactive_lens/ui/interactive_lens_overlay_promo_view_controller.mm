// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/interactive_lens_overlay_promo_view_controller.h"

#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view.h"
#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/lens_overlay_promo_container_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Corner radius for the top two corners of the Lens view.
const CGFloat kLensViewCornerRadius = 45.0;
// Static image assets.
NSString* const kLensImageName = @"mountain_webpage";
// Multiplier for the top padding for the Lens image.
const CGFloat kLensImagePaddingMultiplier = 0.14;
// Margins for the Lens view.
const CGFloat kLensViewTopMargin = 40.0;
const CGFloat kLensViewHorizontalMargin = 20.0;
// Height multipliers for the Lens view.
const CGFloat kLensViewMinHeightMultiplier = 0.4;
const CGFloat kLensViewMaxHeightMultiplier = 1.45;
// Top margin for tip bubble.
const CGFloat kBubbleViewTopMargin = 10.0;
// Top margin for scroll view.
const CGFloat kScrollViewTopMargin = 45.0;
// Animation duration for the tip bubble.
const CGFloat kBubbleViewAnimationDuration = 0.3;
// Margin below the action button.
const CGFloat kButtonBottomMargin = 45.0;
}  // namespace

@interface InteractiveLensOverlayPromoViewController () <
    LensOverlayPromoContainerViewControllerDelegate>

@end

@implementation InteractiveLensOverlayPromoViewController {
  // The container view for the static background image that sits beind the Lens
  // view.
  UIView* _backgroundContainerView;
  // The static background image view that sits inside _backgroundContainerView.
  UIImageView* _backgroundImageView;
  // View for the tip bubble.
  BubbleView* _bubbleView;
  // View controller for the interactive Lens instance.
  LensOverlayPromoContainerViewController* _lensViewController;
  // Scroll view containing the screen's title and subtitle.
  UIScrollView* _textScrollView;
  // Bottom anchor constraint for the tip bubble. The bubble should be
  // constrained to the lens view, but kept within the top padding area of the
  // Lens image.
  NSLayoutConstraint* _bubbleViewBottomConstraint;
  // Whether the bubble is currently being hidden.
  BOOL _isBubbleHiding;
  // The primary action button.
  UIButton* _actionButton;
  // The button's centered constraint for its initial state.
  NSLayoutConstraint* _buttonCenteredConstraint;
  // Whether the action button has been transformed to the primary style.
  BOOL _buttonTransformed;
  // The footer view containing the action button.
  UIView* _footerContainerView;
  // A list of gesture recognizers that were disabled.
  NSMutableArray<UIGestureRecognizer*>* _disabledGestures;
}

@synthesize lensContainerViewController = _lensViewController;
@synthesize lensSearchImage = _lensSearchImage;

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _lensViewController =
        [[LensOverlayPromoContainerViewController alloc] init];
    _lensViewController.delegate = self;
    _disabledGestures = [[NSMutableArray alloc] init];
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
  _footerContainerView = [self footerContainerView];
  [view addSubview:_footerContainerView];
  AddSameConstraintsToSides(
      _footerContainerView, view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);

  // Add the container for the background image view.
  _backgroundContainerView = [[UIView alloc] init];
  _backgroundContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _backgroundContainerView.clipsToBounds = YES;
  _backgroundContainerView.layer.cornerRadius = kLensViewCornerRadius;
  _backgroundContainerView.layer.maskedCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
  [view addSubview:_backgroundContainerView];

  // Create the background image view and constrain it to its container.
  _backgroundImageView = [[UIImageView alloc] init];
  _backgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _backgroundImageView.contentMode = UIViewContentModeScaleAspectFill;
  _backgroundImageView.layer.cornerRadius = kLensViewCornerRadius;
  _backgroundImageView.layer.maskedCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
  _backgroundImageView.layer.borderWidth = 0.5;
  _backgroundImageView.layer.borderColor =
      [UIColor colorNamed:kGrey300Color].CGColor;
  [_backgroundContainerView addSubview:_backgroundImageView];
  AddSameConstraintsToSides(
      _backgroundImageView, _backgroundContainerView,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kTop);

  // Add and constrain the Lens view.
  [_lensViewController willMoveToParentViewController:self];
  [self addChildViewController:_lensViewController];
  UIView* lensView = _lensViewController.view;
  lensView.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:lensView];

  // Make the background image view should be the same size/position as the lens
  // view since it will sit directly behind.
  AddSameConstraints(_backgroundContainerView, lensView);

  NSLayoutConstraint* lensViewTopAnchor =
      [lensView.topAnchor constraintEqualToAnchor:_textScrollView.bottomAnchor
                                         constant:kLensViewTopMargin];
  lensViewTopAnchor.priority = UILayoutPriorityDefaultLow;
  [NSLayoutConstraint activateConstraints:@[
    // The Lens view has both a minimum possible height (relative to the height
    // of its superview) and a maximum possible height (relative to the height
    // of the Lens overlay image asset).
    lensViewTopAnchor,
    [lensView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:view.heightAnchor
                                  multiplier:kLensViewMinHeightMultiplier],
    [lensView.heightAnchor
        constraintLessThanOrEqualToAnchor:lensView.widthAnchor
                               multiplier:kLensViewMaxHeightMultiplier],
    [lensView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor
                       constant:kLensViewHorizontalMargin],
    [lensView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor
                       constant:-kLensViewHorizontalMargin],
    [lensView.bottomAnchor
        constraintEqualToAnchor:_footerContainerView.topAnchor],
    [lensView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_textScrollView.bottomAnchor
                                    constant:kLensViewTopMargin],
  ]];
  [_lensViewController didMoveToParentViewController:self];

  // Create and constrain the bubble view so that it is pinned to the Lens view
  // and always below the title/subtitle.
  _bubbleView = [self bubbleView];
  [view addSubview:_bubbleView];
  CGSize bubbleViewPreferredHeight = [_bubbleView
      sizeThatFits:CGSizeMake(view.bounds.size.width, CGFLOAT_MAX)];
  _bubbleViewBottomConstraint =
      [_bubbleView.bottomAnchor constraintEqualToAnchor:lensView.topAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [_bubbleView.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],
    [_bubbleView.leadingAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.leadingAnchor],
    [_bubbleView.trailingAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.trailingAnchor],
    [_bubbleView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_textScrollView.bottomAnchor
                                    constant:kBubbleViewTopMargin],
    [_bubbleView.heightAnchor
        constraintEqualToConstant:bubbleViewPreferredHeight.height],
    _bubbleViewBottomConstraint,
  ]];

  [self startBubbleAnimation];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  for (UIGestureRecognizer* gesture in _disabledGestures) {
    gesture.enabled = YES;
  }
  [_disabledGestures removeAllObjects];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // Anchor the bubble to the top of the Lens image container. The 0.7
  // multiplier ensures the bubble appears within the top padded area of the
  // image (slightly below the middle of the padded area).
  _bubbleViewBottomConstraint.constant = [self lensImageTopPadding] * 0.7;
  if (!_lensSearchImage) {
    _lensSearchImage = [self createLensSearchImage];
    _backgroundImageView.image = _lensSearchImage;
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_textScrollView flashScrollIndicators];

  // Disable the presentation controller's pan gesture.
  // This prevents the sheet's pull-down gesture from interfering with
  // the drawing functionality within the Lens view.
  UIView* presentedView =
      self.navigationController.presentationController.presentedView;
  for (UIGestureRecognizer* gesture in presentedView.gestureRecognizers) {
    if ([gesture isKindOfClass:[UIPanGestureRecognizer class]] &&
        gesture.enabled) {
      [_disabledGestures addObject:gesture];
      gesture.enabled = NO;
    }
  }
}

#pragma mark - LensOverlayPromoContainerViewControllerDelegate

- (void)lensOverlayPromoContainerViewControllerDidBeginInteraction:
    (LensOverlayPromoContainerViewController*)viewController {
  [self handleHideBubbleAnimation];
}

- (void)lensOverlayPromoContainerViewControllerDidEndInteraction:
    (LensOverlayPromoContainerViewController*)viewController {
  [self transformButtonToPrimaryAction];
}

#pragma mark - Private

// Returns a new image with the Lens search image padded at the top with white
// space.
- (UIImage*)createLensSearchImage {
  UIImage* image = [UIImage imageNamed:kLensImageName];
  CGFloat padding = [self lensImageTopPadding];
  CGSize newSize = CGSizeMake(image.size.width, image.size.height + padding);
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.opaque = YES;
  format.scale = image.scale;
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:newSize format:format];

  UIImage* newImage = [renderer
      imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
        [[UIColor whiteColor] setFill];
        [rendererContext
            fillRect:CGRectMake(0, 0, newSize.width, newSize.height)];
        [image drawInRect:CGRectMake(0, padding, image.size.width,
                                     image.size.height)];
      }];

  return newImage;
}

// The height of the white space above the Lens search image, relative to screen
// size.
- (CGFloat)lensImageTopPadding {
  return self.view.bounds.size.height * kLensImagePaddingMultiplier;
}

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

  _buttonCenteredConstraint = [button.centerXAnchor
      constraintEqualToAnchor:footerContainerView.centerXAnchor];

  [NSLayoutConstraint activateConstraints:@[
    _buttonCenteredConstraint,
    [button.topAnchor constraintEqualToAnchor:separatorLine.bottomAnchor
                                     constant:kButtonVerticalInsets],
    [button.bottomAnchor
        constraintEqualToAnchor:footerContainerView.bottomAnchor
                       constant:-kButtonBottomMargin],
  ]];

  return footerContainerView;
}

// Creates and returns the action button.
- (UIButton*)buttonView {
  _actionButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.background.backgroundColor = [UIColor clearColor];
  buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  _actionButton.configuration = buttonConfiguration;
  NSString* buttonTitle = l10n_util::GetNSString(
      IDS_IOS_INTERACTIVE_LENS_OVERLAY_PROMO_SKIP_BUTTON);
  SetConfigurationTitle(_actionButton, buttonTitle);
  SetConfigurationFont(_actionButton,
                       [UIFont preferredFontForTextStyle:UIFontTextStyleBody]);

  _actionButton.translatesAutoresizingMaskIntoConstraints = NO;
  _actionButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  _actionButton.pointerInteractionEnabled = YES;
  _actionButton.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();
  [_actionButton addTarget:self
                    action:@selector(buttonTapped)
          forControlEvents:UIControlEventTouchUpInside];
  return _actionButton;
}

// Handles taps on the action button.
- (void)buttonTapped {
  [self.delegate didTapContinueButton];
}

// Transforms the action button from its initial plain style to a primary
// action button style.
- (void)transformButtonToPrimaryAction {
  if (_buttonTransformed) {
    return;
  }
  _buttonTransformed = YES;

  // Create a temporary primary action button to copy its configuration.
  UIButton* primaryButton = PrimaryActionButton(YES);
  _actionButton.configuration = primaryButton.configuration;
  NSString* continueButtonTitle = l10n_util::GetNSString(
      IDS_IOS_INTERACTIVE_LENS_OVERLAY_PROMO_START_BROWSING_BUTTON);
  SetConfigurationTitle(_actionButton, continueButtonTitle);

  // Update constraints to make the button full-width.
  _buttonCenteredConstraint.active = NO;
  UILayoutGuide* widthLayoutGuide =
      AddPromoStyleWidthLayoutGuide(_footerContainerView);
  [NSLayoutConstraint activateConstraints:@[
    [_actionButton.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor],
    [_actionButton.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor],
  ]];
}

// Hides the bubble view with a fade-out effect.
- (void)handleHideBubbleAnimation {
  // Don't do anything if the view is already hidden or fading out.
  if (_isBubbleHiding) {
    return;
  }
  _isBubbleHiding = YES;

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kBubbleViewAnimationDuration
      animations:^{
        [weakSelf setBubbleAlphaToZero];
      }
      completion:^(BOOL finished) {
        [weakSelf hideBubbleAfterAnimation];
      }];
}

// Sets the bubble view's alpha to 0. To be used in an animation block.
- (void)setBubbleAlphaToZero {
  _bubbleView.alpha = 0;
}

// Hides the bubble view. To be used as an animation completion.
- (void)hideBubbleAfterAnimation {
  _bubbleView.hidden = YES;
  _isBubbleHiding = NO;
}

@end
