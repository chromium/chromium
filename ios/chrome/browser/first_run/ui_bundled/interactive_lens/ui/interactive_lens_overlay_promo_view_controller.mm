// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/interactive_lens_overlay_promo_view_controller.h"

#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view.h"
#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/lens_overlay_promo_container_view_controller.h"
#import "ios/chrome/common/ui/button_stack/button_stack_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Static image assets.
NSString* const kLensImageName = @"mountain_webpage";
NSString* const kHUDImageName = @"lens_overlay_hud";
// Corner radius for the top two corners of the Lens view.
const CGFloat kLensViewCornerRadius = 45.0;
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
const CGFloat kButtonBottomMargin = 20.0;
// Margin above the HUD view.
const CGFloat kHUDViewTopMargin = 20.0;
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
  // The heads-up display view that sits on top of the Lens view.
  UIImageView* _hudView;
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
  ChromeButton* _actionButton;
  // The footer view containing the action button.
  UIView* _footerContainerView;
  // The separator line in the footer.
  UIView* _separatorLine;
  // A list of gesture recognizers that were disabled.
  NSMutableArray<UIGestureRecognizer*>* _disabledGestures;
  // Whether the user has interacted with the Lens view.
  BOOL _interaction;
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
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  [self setUpViews];
  [self setUpConstraints];

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

#pragma mark - LensInteractivePromoResultsPagePresenterDelegate

- (void)lensInteractivePromoResultsPagePresenterWillPresentResults:
    (LensInteractivePromoResultsPagePresenter*)presenter {
  [self showHUDView];
}

- (void)lensInteractivePromoResultsPagePresenterDidDismissResults:
    (LensInteractivePromoResultsPagePresenter*)presenter {
  _hudView.hidden = YES;
}

#pragma mark - LensOverlayPromoContainerViewControllerDelegate

- (void)lensOverlayPromoContainerViewControllerDidBeginInteraction:
    (LensOverlayPromoContainerViewController*)viewController {
  [self handleHideBubbleAnimation];
  _interaction = YES;
}

- (void)lensOverlayPromoContainerViewControllerDidEndInteraction:
    (LensOverlayPromoContainerViewController*)viewController {
  [self transformButtonToPrimaryAction];
}

#pragma mark - Private

// Sets up the initial view hierarchy.
- (void)setUpViews {
  // Add a gradient to the background.
  GradientView* gradientView = [[GradientView alloc]
      initWithTopColor:[UIColor colorNamed:kPrimaryBackgroundColor]
           bottomColor:[UIColor colorNamed:kSecondaryBackgroundColor]];
  gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:gradientView];
  AddSameConstraints(gradientView, self.view);

  _textScrollView = [self textScrollView];
  [self.view addSubview:_textScrollView];

  _footerContainerView = [self footerContainerView];
  [self.view addSubview:_footerContainerView];

  _backgroundContainerView = [[UIView alloc] init];
  _backgroundContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _backgroundContainerView.clipsToBounds = YES;
  _backgroundContainerView.layer.cornerRadius = kLensViewCornerRadius;
  _backgroundContainerView.layer.maskedCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
  [self.view addSubview:_backgroundContainerView];

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

  [_lensViewController willMoveToParentViewController:self];
  [self addChildViewController:_lensViewController];
  UIView* lensView = _lensViewController.view;
  lensView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:lensView];
  [_lensViewController didMoveToParentViewController:self];

  _bubbleView = [self bubbleView];
  [self.view addSubview:_bubbleView];
}

// Sets up the layout constraints for the view hierarchy.
- (void)setUpConstraints {
  UIView* view = self.view;
  UILayoutGuide* widthLayoutGuide = AddButtonStackContentWidthLayoutGuide(view);

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
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;

  AddSameConstraintsToSides(
      _footerContainerView, view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);

  [NSLayoutConstraint activateConstraints:@[
    [_separatorLine.topAnchor
        constraintEqualToAnchor:_footerContainerView.topAnchor],
    [_separatorLine.leadingAnchor
        constraintEqualToAnchor:_footerContainerView.leadingAnchor],
    [_separatorLine.trailingAnchor
        constraintEqualToAnchor:_footerContainerView.trailingAnchor],
    [_separatorLine.heightAnchor constraintEqualToConstant:1.0]
  ]];

  UILayoutGuide* footerWidthLayoutGuide =
      AddButtonStackContentWidthLayoutGuide(_footerContainerView);
  [NSLayoutConstraint activateConstraints:@[
    [_actionButton.leadingAnchor
        constraintEqualToAnchor:footerWidthLayoutGuide.leadingAnchor],
    [_actionButton.trailingAnchor
        constraintEqualToAnchor:footerWidthLayoutGuide.trailingAnchor],
    [_actionButton.bottomAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.bottomAnchor
                       constant:-kButtonBottomMargin],
    [_actionButton.topAnchor constraintEqualToAnchor:_separatorLine.bottomAnchor
                                            constant:kButtonVerticalInsets]
  ]];

  AddSameConstraintsToSides(
      _backgroundImageView, _backgroundContainerView,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kTop);

  UIView* lensView = _lensViewController.view;
  AddSameConstraints(_backgroundContainerView, lensView);

  NSLayoutConstraint* lensViewTopAnchor =
      [lensView.topAnchor constraintEqualToAnchor:_textScrollView.bottomAnchor
                                         constant:kLensViewTopMargin];
  lensViewTopAnchor.priority = UILayoutPriorityDefaultLow;
  [NSLayoutConstraint activateConstraints:@[
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
}

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
  footerContainerView.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];

  _separatorLine = [[UIView alloc] init];
  _separatorLine.translatesAutoresizingMaskIntoConstraints = NO;
  _separatorLine.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  [footerContainerView addSubview:_separatorLine];

  _actionButton = [self createActionButton];
  [footerContainerView addSubview:_actionButton];

  return footerContainerView;
}

// Creates and presents the HUD view.
- (void)showHUDView {
  if (_hudView) {
    return;
  }

  // Add and constrain the HUD view.
  _hudView =
      [[UIImageView alloc] initWithImage:[UIImage imageNamed:kHUDImageName]];
  _hudView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_hudView];
  UIView* lensView = _lensViewController.view;
  [NSLayoutConstraint activateConstraints:@[
    [_hudView.topAnchor constraintEqualToAnchor:lensView.topAnchor
                                       constant:kHUDViewTopMargin],
  ]];
  AddSameConstraintsToSides(_hudView, lensView,
                            LayoutSides::kLeading | LayoutSides::kTrailing);

  // Animate the HUD sliding down from the top.
  CGFloat hudHeight = _hudView.image.size.height;
  _hudView.transform = CGAffineTransformMakeTranslation(0, -hudHeight);
  __weak __typeof(_hudView) weakHudView = _hudView;
  [UIView animateWithDuration:0.3
                        delay:0.1
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     weakHudView.transform = CGAffineTransformIdentity;
                   }
                   completion:nil];
}

// Creates and returns the action button.
- (ChromeButton*)createActionButton {
  ChromeButton* actionButton =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStyleSecondary];
  NSString* buttonTitle = l10n_util::GetNSString(
      IDS_IOS_INTERACTIVE_LENS_OVERLAY_PROMO_SKIP_BUTTON);
  actionButton.title = buttonTitle;

  [actionButton addTarget:self
                   action:@selector(buttonTapped)
         forControlEvents:UIControlEventTouchUpInside];
  return actionButton;
}

// Handles taps on the action button.
- (void)buttonTapped {
  [self.delegate didTapContinueButtonWithInteraction:_interaction];
}

// Transforms the action button from its initial plain style to a primary
// action button style.
- (void)transformButtonToPrimaryAction {
  if (_actionButton.style == ChromeButtonStylePrimary) {
    return;
  }

  _actionButton.style = ChromeButtonStylePrimary;
  NSString* continueButtonTitle = l10n_util::GetNSString(
      IDS_IOS_INTERACTIVE_LENS_OVERLAY_PROMO_START_BROWSING_BUTTON);
  _actionButton.title = continueButtonTitle;
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
