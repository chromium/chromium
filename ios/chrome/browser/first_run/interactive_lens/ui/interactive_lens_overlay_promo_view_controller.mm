// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/interactive_lens/ui/interactive_lens_overlay_promo_view_controller.h"

#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view.h"
#import "ios/chrome/browser/first_run/interactive_lens/ui/lens_overlay_promo_container_view_controller.h"
#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"
#import "ios/chrome/common/ui/button_stack/button_stack_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Static image assets.
NSString* const kLensImageName = @"mountain_webpage";
NSString* const kFakeWebpageImageName = @"fake_webpage";
NSString* const kHUDImageName = @"lens_overlay_hud";
NSString* const kCirclingHandAnimationName = @"cursor+line";
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
  // The heads-up display view that sits on top of the Lens view.
  UIImageView* _hudView;
  // Layout guide to handle the static positioning for the bubble view. The
  // bubble view has an animation, and that animation uses constraints between
  // the bubble view and this layout guide.
  UIView* _bubbleContainerView;
  // View for the tip bubble.
  BubbleView* _bubbleView;
  // Bottom anchor constraint for the tip bubble's positioning container view.
  // The bubble should be constrained to the lens view, but kept within the top
  // padding area of the Lens image.
  NSLayoutConstraint* _bubbleContainerViewBottomConstraint;
  // Bottom anchor constraint for the bubble itself, that can be animated to
  // move the bubble inside its container.
  NSLayoutConstraint* _bubbleViewBottomInnerAnimationConstraint;
  // View controller for the interactive Lens instance.
  LensOverlayPromoContainerViewController* _lensViewController;
  // Constraint to make sure the lens view intially shows the entire image.
  NSLayoutConstraint* _lensViewHeightConstraint;
  // Container view for the entire lens section. This allows placing a view
  // containing the fake webpage content below the actual lens image view.
  UIView* _lensContainerView;
  // Container containing the fake webpage image. This is necessary because
  // UIImageView doesn't support scaling + top aligning an image.
  UIView* _fakeWebpageContainerView;
  // Scroll view containing the screen's title and subtitle.
  UIScrollView* _textScrollView;
  // Whether the bubble is currently being hidden.
  BOOL _isBubbleHiding;
  // Lottie object for the circling hand animation.
  id<LottieAnimation> _circlingHandAnimation;
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
  // The remaining number of times the bubble animation should run.
  int _bubbleAnimationCyclesRemaining;
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
  _bubbleContainerViewBottomConstraint.constant =
      [self lensImageTopPadding] * 0.7;
  if (!_lensSearchImage) {
    _lensSearchImage = [self createLensSearchImage];
    _lensViewHeightConstraint.constant = _lensSearchImage.size.height;
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

  [self startBubbleAnimation];
  [_circlingHandAnimation play];
}

#pragma mark - LensInteractivePromoResultsPagePresenterDelegate

- (void)lensInteractivePromoResultsPagePresenterWillPresentResults:
    (LensInteractivePromoResultsPagePresenter*)presenter {
  [self showHUDView];
  // Shrink the fake webpage to 0 height and allow the lens view to grow.
  [_fakeWebpageContainerView.heightAnchor constraintEqualToConstant:0].active =
      YES;
  _lensViewHeightConstraint.active = NO;
}

- (void)lensInteractivePromoResultsPagePresenterDidDismissResults:
    (LensInteractivePromoResultsPagePresenter*)presenter {
  _hudView.hidden = YES;
}

#pragma mark - LensOverlayPromoContainerViewControllerDelegate

- (void)lensOverlayPromoContainerViewControllerDidBeginInteraction:
    (LensOverlayPromoContainerViewController*)viewController {
  [self handleHideBubbleAnimation];
  [self hideCirclingHandAnimation];
  _interaction = YES;
}

- (void)lensOverlayPromoContainerViewControllerDidEndInteraction:
    (LensOverlayPromoContainerViewController*)viewController {
  [self transformButtonToPrimaryAction];
  // Further interactions with the lens view can be janky and are unnecessary.
  _lensViewController.view.userInteractionEnabled = NO;
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

  _lensContainerView = [[UIView alloc] init];
  _lensContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_lensContainerView];

  _fakeWebpageContainerView = [self fakeWebpageContainerView];
  [_lensContainerView addSubview:_fakeWebpageContainerView];

  [_lensViewController willMoveToParentViewController:self];
  [self addChildViewController:_lensViewController];
  UIView* lensView = _lensViewController.view;
  lensView.translatesAutoresizingMaskIntoConstraints = NO;
  [_lensContainerView addSubview:lensView];
  [_lensViewController didMoveToParentViewController:self];

  _bubbleContainerView = [[UIView alloc] init];
  _bubbleContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_bubbleContainerView];

  _bubbleView = [self bubbleView];
  [_bubbleContainerView addSubview:_bubbleView];

  _circlingHandAnimation = [self createCirclingHandAnimation];
  [self.view addSubview:_circlingHandAnimation.animationView];
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
  heightConstraint.priority = UILayoutPriorityDefaultHigh;
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
                                            constant:kButtonVerticalInsets],
  ]];

  UIView* lensView = _lensViewController.view;

  // The lens view is in the top portion of the container.
  AddSameConstraintsToSides(
      _lensContainerView, lensView,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kTop);
  // The fake webpage view is in the bottom portion of the container.
  AddSameConstraintsToSides(
      _lensContainerView, _fakeWebpageContainerView,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
  // This will be filled with the actual image height when the image is loaded.
  _lensViewHeightConstraint =
      [lensView.heightAnchor constraintEqualToConstant:0];

  [NSLayoutConstraint activateConstraints:@[
    _lensViewHeightConstraint,
    [lensView.bottomAnchor
        constraintEqualToAnchor:_fakeWebpageContainerView.topAnchor],
  ]];

  NSLayoutConstraint* lensViewTopAnchor = [_lensContainerView.topAnchor
      constraintEqualToAnchor:_textScrollView.bottomAnchor
                     constant:kLensViewTopMargin];
  lensViewTopAnchor.priority = UILayoutPriorityDefaultLow - 1;
  NSLayoutConstraint* maximizeLensViewHeight = [_lensContainerView.heightAnchor
      constraintEqualToAnchor:view.heightAnchor];
  maximizeLensViewHeight.priority = UILayoutPriorityDefaultHigh - 1;
  [NSLayoutConstraint activateConstraints:@[
    lensViewTopAnchor,
    maximizeLensViewHeight,
    [_lensContainerView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:view.heightAnchor
                                  multiplier:kLensViewMinHeightMultiplier],
    [_lensContainerView.heightAnchor
        constraintLessThanOrEqualToAnchor:_lensContainerView.widthAnchor
                               multiplier:kLensViewMaxHeightMultiplier],
    [_lensContainerView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor
                       constant:kLensViewHorizontalMargin],
    [_lensContainerView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor
                       constant:-kLensViewHorizontalMargin],
    [_lensContainerView.bottomAnchor
        constraintEqualToAnchor:_footerContainerView.topAnchor],
    [_lensContainerView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_textScrollView.bottomAnchor
                                    constant:kLensViewTopMargin],
  ]];

  CGSize bubbleViewPreferredHeight = [_bubbleView
      sizeThatFits:CGSizeMake(view.bounds.size.width, CGFLOAT_MAX)];
  _bubbleContainerViewBottomConstraint = [_bubbleContainerView.bottomAnchor
      constraintEqualToAnchor:_lensContainerView.topAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [_bubbleContainerView.centerXAnchor
        constraintEqualToAnchor:view.centerXAnchor],
    [_bubbleContainerView.leadingAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.leadingAnchor],
    [_bubbleContainerView.trailingAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.trailingAnchor],
    [_bubbleContainerView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_textScrollView.bottomAnchor
                                    constant:kBubbleViewTopMargin],
    [_bubbleContainerView.heightAnchor
        constraintEqualToAnchor:_bubbleView.heightAnchor],
    _bubbleContainerViewBottomConstraint,
  ]];

  _bubbleViewBottomInnerAnimationConstraint = [_bubbleContainerView.bottomAnchor
      constraintEqualToAnchor:_bubbleView.bottomAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [_bubbleView.leadingAnchor
        constraintEqualToAnchor:_bubbleContainerView.leadingAnchor],
    [_bubbleView.trailingAnchor
        constraintEqualToAnchor:_bubbleContainerView.trailingAnchor],
    _bubbleViewBottomInnerAnimationConstraint,
    [_bubbleView.heightAnchor
        constraintEqualToConstant:bubbleViewPreferredHeight.height],
  ]];

  AddSameConstraints(_lensContainerView, _circlingHandAnimation.animationView);
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

// Creates and returns the LottieAnimation view for the circling hand animation.
- (id<LottieAnimation>)createCirclingHandAnimation {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = kCirclingHandAnimationName;
  // The hand should circle maximum 3 times, which is built into the animation
  // already.
  config.shouldLoop = NO;
  id<LottieAnimation> animation =
      ios::provider::GenerateLottieAnimation(config);

  animation.animationView.translatesAutoresizingMaskIntoConstraints = NO;
  animation.animationView.contentMode = UIViewContentModeScaleAspectFit;
  animation.animationView.userInteractionEnabled = NO;

  return animation;
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
    [textStack.leadingAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.leadingAnchor],
    [textStack.trailingAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.trailingAnchor],
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

// Creates and returns the container view holding the fake webpage image.
- (UIView*)fakeWebpageContainerView {
  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  containerView.clipsToBounds = YES;

  UIImageView* fakeWebpageImageView = [[UIImageView alloc] init];
  fakeWebpageImageView.translatesAutoresizingMaskIntoConstraints = NO;
  fakeWebpageImageView.contentMode = UIViewContentModeScaleAspectFill;
  [containerView addSubview:fakeWebpageImageView];

  UIImage* fakeWebpageImage = [UIImage imageNamed:kFakeWebpageImageName];
  fakeWebpageImageView.image = fakeWebpageImage;

  // The image should scale to fill the entire width, but also be top aligned,
  // with any extra content on the bottom cropped. Unfortunately, UIKit does not
  // have a content mode of "UIViewContentModeScaleAspectFill +
  // UIViewContentModeTop." Instead, the image view has a constrained aspect
  // ratio, so it can grow in height, and a content mode of ScaleAspectFill, so
  // the image takes up the full space. And then, it's aligned to the top of the
  // container, which clips the excess at the bottom.
  AddSameConstraintsToSides(
      containerView, fakeWebpageImageView,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kTop);
  [fakeWebpageImageView.widthAnchor
      constraintEqualToAnchor:fakeWebpageImageView.heightAnchor
                   multiplier:fakeWebpageImage.size.width /
                              fakeWebpageImage.size.height]
      .active = YES;

  return containerView;
}

// Starts the animation for the tip bubble view.
- (void)startBubbleAnimation {
  _bubbleAnimationCyclesRemaining = 3;
  [self runBubbleUpAnimation];
}

// Run the portion of the animation to move the bubble up.
- (void)runBubbleUpAnimation {
  _bubbleViewBottomInnerAnimationConstraint.constant = 18;

  __weak __typeof(self) weakSelf = self;
  __weak __typeof(_bubbleContainerView) weakContainerView =
      _bubbleContainerView;
  [UIView animateWithDuration:1.5
      delay:0.0
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        [weakContainerView layoutIfNeeded];
      }
      completion:^(BOOL success) {
        if (!success) {
          return;
        }
        [weakSelf runBubbleDownAnimation];
      }];
}

// Run the portion of the animation to move the bubble down. And if the
// animation has now shown enough times, stop.
- (void)runBubbleDownAnimation {
  _bubbleViewBottomInnerAnimationConstraint.constant = 0;

  __weak __typeof(self) weakSelf = self;
  __weak __typeof(_bubbleContainerView) weakContainerView =
      _bubbleContainerView;
  [UIView animateWithDuration:1.5
      delay:0.0
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        [weakContainerView layoutIfNeeded];
      }
      completion:^(BOOL success) {
        [weakSelf completeBubbleDownAnimation:success];
      }];
}

// Completion block for the bubble down animation.
- (void)completeBubbleDownAnimation:(BOOL)success {
  if (!success) {
    return;
  }
  _bubbleAnimationCyclesRemaining -= 1;
  if (_bubbleAnimationCyclesRemaining <= 0) {
    return;
  }
  [self runBubbleUpAnimation];
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

// Hides the circling hand animation.
- (void)hideCirclingHandAnimation {
  _circlingHandAnimation.animationView.hidden = YES;
}

@end
