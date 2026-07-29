// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_sharing/sharing_status_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_constants.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_sharing/sharing_status_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Progress bar dimensions.
const CGFloat kProgressBarWidth = 100.0;
const CGFloat kProgressBarHeight = 30.0;
const CGFloat kProgressBarCircleDiameter = 3.0;
const CGFloat kProgressBarCircleSpacing = 2.0;
const NSInteger kProgressBarCirclesAmount = 20;

// Loaded images size dimensions.
const CGFloat kLockSymbolPointSize = 22.0;
const CGFloat kFaviconContainerSize = 30.0;
const CGFloat kFaviconSize = 22.0;
const CGFloat kProfileImageSize = 60.0;

// Spacing and padding constraints.
const CGFloat kVerticalSpacing = 16.0;
const CGFloat kTopPadding = 20.0;
const CGFloat kBottomPadding = 42.0;
const CGFloat kHorizontalPadding = 16.0;
const CGFloat kFaviconProfileImageVerticalOverlap = 10.0;

// Durations of specific parts of the animation in seconds.
const CGFloat kImagesSlidingOutDelay = 0.35;
const CGFloat kImagesSlidingOutDuration = 0.5;
const CGFloat kLockAppearingDuration = 0.15;
const CGFloat kProgressBarLoadingDuration = 3.25;
const CGFloat kImagesSlidingInDuration = 0.5;
const CGFloat kFaviconAppearingDelay = 0.1;
const CGFloat kFaviconAppearingDuration = 0.15;
const CGFloat kSharingCancelledDuration = 0.5;
const CGFloat kStatusTransitionDuration = 0.25;

// Distance by which the profile images x-center should be away from the middle
// of the view in different parts of the animation.
const CGFloat kImagesSlidedOutCenterXConstant = 78;
const CGFloat kImagesSlidedInCenterXConstant = 27;

// Tags marking parts of string that should have a bold font.
NSString* const kBeginBoldTag = @"BEGIN_BOLD[ \t]*";
NSString* const kEndBoldTag = @"[ \t]*END_BOLD";

// Accessibility identifiers of text views with links.
NSString* const kSharingStatusFooterId = @"SharingStatusViewFooter";

}  // namespace

@interface SharingStatusViewController () <UITextViewDelegate>
@end

@implementation SharingStatusViewController {
  // Container view for the animation.
  UIView* _animationView;

  // Profile image of the sender.
  UIImageView* _senderImageView;
  UIImage* _senderImage;

  // Profile image of the recipient (or merged avatar of multiple recipients).
  UIImageView* _recipientImageView;
  UIImage* _recipientImage;

  // Lock image displayed in the animation.
  UIImageView* _lockImage;

  // Rectangle view with fixed length and height containing fixed amount of
  // circles.
  UIView* _progressBarView;

  // The container for the favicon view that is displayed below the recipient and
  // sender images in successful status view.
  FaviconContainerView* _faviconContainerView;

  // Stack view containing animation container view, title, subtitle and footer.
  UIStackView* _stackView;

  // Animates profile image of the sender sliding to the left and profile images
  // of recipients sliding to the right.
  UIViewPropertyAnimator* _imagesSlidingOutAnimation;

  // Animates lock appearing in the middle between profile images.
  UIViewPropertyAnimator* _lockAppearingAnimation;

  // Animates the progress bar going from the left to right image.
  UIViewPropertyAnimator* _progressBarLoadingAnimation;

  // Animates progress bar and lock disappearing and profile images sliding to the
  // middle.
  UIViewPropertyAnimator* _imagesSlidingInAnimation;

  // Animates favicon appearing below recipient and sender image.
  UIViewPropertyAnimator* _faviconAppearingAnimation;

  // Animates profile images sliding to the middle on cancel button tap.
  UIViewPropertyAnimator* _sharingCancelledAnimation;

  // Contains the information that sharing is in progress at first and then is
  // modified to convey the result status.
  UILabel* _titleLabel;

  // Subtitle string that will be displayed when the sharing is succesful.
  NSString* _subtitleString;

  // Footer string that will be displayed when the sharing is succesful.
  NSString* _footerString;

  // The button that cancels the sharing process.
  UIButton* _cancelButton;

  // Subtitle text view displayed inside stack view.
  UITextView* _subtitleTextView;

  // Footer text view displayed inside stack view.
  UITextView* _footerTextView;

  // The button that dismisses the sharing status view when done.
  UIButton* _doneButton;

  // Url of the site for which the password is being shared.
  GURL _URL;

  // CenterX constraints for the images of sender and recipients.
  NSLayoutConstraint* _senderImageCenterXConstraint;
  NSLayoutConstraint* _recipientImageCenterXConstraint;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* view = self.view;
  view.accessibilityIdentifier = kSharingStatusViewID;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  // Add vertical stack view for the animation and all labels.
  _animationView = [self createAnimationContainerView];
  _titleLabel = [self createTitleLabel];
  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _animationView, _titleLabel
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = kVerticalSpacing;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  _stackView = verticalStack;
  [view addSubview:verticalStack];

  // Add cancel button below the stack.
  _cancelButton = [self createCancelButton];
  [view addSubview:_cancelButton];

  [NSLayoutConstraint activateConstraints:@[
    // Vertical stack constraints.
    [verticalStack.topAnchor constraintEqualToAnchor:view.topAnchor
                                            constant:kTopPadding],
    [verticalStack.leadingAnchor constraintEqualToAnchor:view.leadingAnchor
                                                constant:kHorizontalPadding],
    [verticalStack.trailingAnchor constraintEqualToAnchor:view.trailingAnchor
                                                 constant:-kHorizontalPadding],
    [verticalStack.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],

    // Cancel button constraints.
    [_cancelButton.topAnchor
        constraintGreaterThanOrEqualToAnchor:verticalStack.bottomAnchor
                                    constant:kVerticalSpacing],
    [_cancelButton.bottomAnchor constraintEqualToAnchor:view.bottomAnchor
                                               constant:-kBottomPadding],
    [_cancelButton.centerXAnchor
        constraintEqualToAnchor:verticalStack.centerXAnchor],
  ]];

  [self createAnimations];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Make sure that the title is focused when the view appears.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  _titleLabel);
  [_imagesSlidingOutAnimation startAnimationAfterDelay:kImagesSlidingOutDelay];
}

- (void)viewDidDisappear:(BOOL)animated {
  // Stop the ongoing animations so that their completion is not called.
  [_imagesSlidingOutAnimation stopAnimation:YES];
  [_lockAppearingAnimation stopAnimation:YES];
  [_progressBarLoadingAnimation stopAnimation:YES];
  [_imagesSlidingInAnimation stopAnimation:YES];
  [super viewDidDisappear:animated];
}

#pragma mark - Public

- (UISheetPresentationControllerDetent*)preferredHeightDetent {
  __typeof(self) __weak weakSelf = self;
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf detentForPreferredHeightInContext:context];
  };
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:@"preferred_height"
                        resolver:resolver];
}

#pragma mark - SharingStatusConsumer

- (void)setSenderImage:(UIImage*)senderImage {
  _senderImage = senderImage;
}

- (void)setRecipientImage:(UIImage*)recipientImage {
  _recipientImage = recipientImage;
}

- (void)setSubtitleString:(NSString*)subtitleString {
  _subtitleString = [subtitleString copy];
}

- (void)setFooterString:(NSString*)footerString {
  _footerString = [footerString copy];
}

- (void)setURL:(const GURL&)URL {
  _URL = URL;
}

#pragma mark - UITextViewDelegate

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  __weak __typeof(self) weakSelf = self;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.delegate changePasswordLinkWasTapped];
  }];
}

#pragma mark - Private

- (CGFloat)detentForPreferredHeightInContext:
    (id<UISheetPresentationControllerDetentResolutionContext>)context {
  UIView* containerView = self.sheetPresentationController.containerView;
  CGFloat width = containerView.bounds.size.width;
  CGSize fittingSize = CGSizeMake(width, UILayoutFittingCompressedSize.height);
  CGFloat height = [self.view systemLayoutSizeFittingSize:fittingSize].height;

  // Measure height without the safeAreaInsets.bottom in portrait orientation on
  // iPhone (as it is added anyway to the result in edge-attached sheets).
  UITraitCollection* traitCollection = context.containerTraitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular) {
    height -= containerView.safeAreaInsets.bottom;
  }
  return height;
}

// Helper for creating sender image view.
- (UIImageView*)createSenderImageView {
  UIImageView* senderImageView =
      [[UIImageView alloc] initWithImage:_senderImage];
  senderImageView.translatesAutoresizingMaskIntoConstraints = NO;
  return senderImageView;
}

// Helper for creating recipient image view.
- (UIImageView*)createRecipientImageView {
  UIImageView* recipientImageView =
      [[UIImageView alloc] initWithImage:_recipientImage];
  recipientImageView.translatesAutoresizingMaskIntoConstraints = NO;
  recipientImageView.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  return recipientImageView;
}

// Helper for creating progress bar view.
- (UIView*)createProgressBarView {
  UIView* progressBarView = [[UIView alloc] init];
  progressBarView.translatesAutoresizingMaskIntoConstraints = NO;
  progressBarView.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  return progressBarView;
}

// Helper for creating the lock image view.
- (UIImageView*)createLockImage {
  UIImageView* lockImage = [[UIImageView alloc]
      initWithImage:SymbolWithPointSize(SymbolLock, kLockSymbolPointSize)];
  lockImage.translatesAutoresizingMaskIntoConstraints = NO;
  lockImage.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  lockImage.hidden = YES;
  return lockImage;
}

// Creates `kProgressBarCirclesAmount` blue circles in the progress bar view.
- (void)createProgressBarSubviews {
  for (NSInteger i = 0; i < kProgressBarCirclesAmount; i++) {
    UIView* circleView =
        [[UIView alloc] initWithFrame:CGRectMake((kProgressBarCircleDiameter +
                                                  kProgressBarCircleSpacing) *
                                                     i,
                                                 kProgressBarHeight / 2,
                                                 kProgressBarCircleDiameter,
                                                 kProgressBarCircleDiameter)];
    circleView.backgroundColor = [UIColor colorNamed:kBlueColor];
    circleView.alpha = 0.0;
    circleView.layer.cornerRadius = kProgressBarCircleDiameter / 2;
    [_progressBarView addSubview:circleView];
  }
}

// Creates favicon view and fetches the actual favicon, while setting the
// default world icon as well as a fallback.
- (FaviconView*)createFaviconView {
  FaviconView* faviconView = [[FaviconView alloc] init];
  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconView.contentMode = UIViewContentModeScaleAspectFill;

  // Use the default world icon as a fallback.
  FaviconAttributes* defaultFaviconAttributes = [FaviconAttributes
      attributesWithImage:[UIImage imageNamed:@"default_world_favicon"]];
  [faviconView configureWithAttributes:defaultFaviconAttributes];

  // Fetch the actual favicon.
  [self.imageDataSource
      faviconForPageURL:[[CrURL alloc] initWithGURL:_URL]
             completion:^(FaviconAttributes* attributes, bool cached) {
               [faviconView configureWithAttributes:attributes];
             }];

  return faviconView;
}

// Creates and returns the container for the favicon view.
- (FaviconContainerView*)createFaviconContainerView {
  FaviconContainerView* faviconContainerView =
      [[FaviconContainerView alloc] init];
  faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconContainerView.hidden = YES;
  return faviconContainerView;
}

// Creates the container view for the animation.
- (UIView*)createAnimationContainerView {
  UIView* animationView = [[UIView alloc] init];
  animationView.translatesAutoresizingMaskIntoConstraints = NO;

  // Add progress bar view.
  _progressBarView = [self createProgressBarView];
  [animationView addSubview:_progressBarView];

  // Add progress bar circles.
  [self createProgressBarSubviews];

  // Add lock image.
  _lockImage = [self createLockImage];
  [_progressBarView addSubview:_lockImage];

  // Add sender profile image.
  _senderImageView = [self createSenderImageView];
  [animationView addSubview:_senderImageView];

  // Add recipient profile image.
  _recipientImageView = [self createRecipientImageView];
  [animationView addSubview:_recipientImageView];

  // Add favicon and its container.
  _faviconContainerView = [self createFaviconContainerView];
  [animationView addSubview:_faviconContainerView];
  FaviconView* faviconView = [self createFaviconView];
  [_faviconContainerView addSubview:faviconView];

  [NSLayoutConstraint activateConstraints:@[
    // Sender image constraints.
    [_senderImageView.topAnchor constraintEqualToAnchor:animationView.topAnchor
                                               constant:kVerticalSpacing],
    [_senderImageView.bottomAnchor
        constraintEqualToAnchor:animationView.bottomAnchor
                       constant:-kVerticalSpacing],
    [_senderImageView.widthAnchor constraintEqualToConstant:kProfileImageSize],
    [_senderImageView.heightAnchor constraintEqualToConstant:kProfileImageSize],

    // Recipient image constraints.
    [_recipientImageView.centerYAnchor
        constraintEqualToAnchor:_senderImageView.centerYAnchor],
    [_recipientImageView.widthAnchor
        constraintEqualToConstant:kProfileImageSize],
    [_recipientImageView.heightAnchor
        constraintEqualToConstant:kProfileImageSize],

    // Progress bar constraints.
    [_progressBarView.centerXAnchor
        constraintEqualToAnchor:animationView.centerXAnchor],
    [_progressBarView.centerYAnchor
        constraintEqualToAnchor:_senderImageView.centerYAnchor],
    [_progressBarView.widthAnchor constraintEqualToConstant:kProgressBarWidth],
    [_progressBarView.heightAnchor constraintEqualToConstant:kProgressBarHeight],

    // Lock image constraints.
    [_lockImage.centerYAnchor
        constraintEqualToAnchor:_senderImageView.centerYAnchor],
    [_lockImage.centerXAnchor
        constraintEqualToAnchor:animationView.centerXAnchor],

    // Favicon constraints.
    [_faviconContainerView.topAnchor
        constraintEqualToAnchor:_senderImageView.bottomAnchor
                       constant:-kFaviconProfileImageVerticalOverlap],
    [_faviconContainerView.centerXAnchor
        constraintEqualToAnchor:animationView.centerXAnchor],
    [_faviconContainerView.widthAnchor
        constraintEqualToConstant:kFaviconContainerSize],
    [_faviconContainerView.heightAnchor
        constraintEqualToConstant:kFaviconContainerSize],
    [faviconView.centerXAnchor
        constraintEqualToAnchor:_faviconContainerView.centerXAnchor],
    [faviconView.centerYAnchor
        constraintEqualToAnchor:_faviconContainerView.centerYAnchor],
    [faviconView.widthAnchor constraintEqualToConstant:kFaviconSize],
    [faviconView.heightAnchor constraintEqualToConstant:kFaviconSize],
  ]];

  _senderImageCenterXConstraint = [_senderImageView.centerXAnchor
      constraintEqualToAnchor:animationView.centerXAnchor];
  _senderImageCenterXConstraint.active = YES;
  _recipientImageCenterXConstraint = [_recipientImageView.centerXAnchor
      constraintEqualToAnchor:animationView.centerXAnchor];
  _recipientImageCenterXConstraint.active = YES;

  return animationView;
}

// Creates title label.
- (UILabel*)createTitleLabel {
  UILabel* title = [[UILabel alloc] init];
  title.numberOfLines = 0;
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_STATUS_PROGRESS_TITLE);
  title.font = CreateDynamicFont(UIFontTextStyleTitle1, UIFontWeightBold);
  title.adjustsFontForContentSizeCategory = YES;
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.textAlignment = NSTextAlignmentCenter;
  title.accessibilityIdentifier = kSharingStatusTitleLabelID;
  return title;
}

// Helper for creating the cancel button
- (UIButton*)createCancelButton {
  UIButton* cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
  cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  [cancelButton setTitle:l10n_util::GetNSString(IDS_CANCEL)
                forState:UIControlStateNormal];
  [cancelButton addTarget:self
                   action:@selector(cancelButtonTapped)
         forControlEvents:UIControlEventTouchUpInside];
  cancelButton.accessibilityIdentifier = kSharingStatusCancelButtonID;
  return cancelButton;
}

// Creates sharing status animations that are started one by one.
- (void)createAnimations {
  UICubicTimingParameters* imagesSlidingTimingParams =
      [[UICubicTimingParameters alloc]
          initWithControlPoint1:CGPointMake(0.7, 0.0)
                  controlPoint2:CGPointMake(0.45, 1.45)];
  _imagesSlidingOutAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kImagesSlidingOutDuration
      timingParameters:imagesSlidingTimingParams];
  __weak __typeof(self) weakSelf = self;
  [_imagesSlidingOutAnimation addAnimations:^{
    [weakSelf setImagesCenterXConstraint:kImagesSlidedOutCenterXConstant];
  }];
  [_imagesSlidingOutAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf startLockAppearingAnimation];
      }];

  _lockAppearingAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kLockAppearingDuration
                 curve:UIViewAnimationCurveEaseIn
            animations:^{
              [weakSelf showLockImage];
            }];
  [_lockAppearingAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf startProgressBarLoadingAnimation];
      }];

  _progressBarLoadingAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kProgressBarLoadingDuration
                 curve:UIViewAnimationCurveLinear
            animations:^{
              [weakSelf animateProgressBarLoading];
            }];
  [_progressBarLoadingAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf startImagesSlidingInAnimation];
      }];

  _imagesSlidingInAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kImagesSlidingInDuration
      timingParameters:imagesSlidingTimingParams];
  [_imagesSlidingInAnimation addAnimations:^{
    [weakSelf animateImagesSlidingIn];
  }];
  [_imagesSlidingInAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf startFaviconAppearingAnimation];
      }];

  _faviconAppearingAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kFaviconAppearingDuration
                 curve:UIViewAnimationCurveEaseIn
            animations:^{
              [weakSelf showFaviconContainer];
            }];
  [_faviconAppearingAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf onFaviconAppearingAnimationCompleted];
      }];

  UICubicTimingParameters* animationCancelledTimingParams =
      [[UICubicTimingParameters alloc]
          initWithControlPoint1:CGPointMake(0.7, -0.45)
                  controlPoint2:CGPointMake(0.45, 1.0)];
  _sharingCancelledAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kSharingCancelledDuration
      timingParameters:animationCancelledTimingParams];
  [_sharingCancelledAnimation addAnimations:^{
    [weakSelf animateSharingCancelled];
  }];
  [_sharingCancelledAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf displayCancelledStatus];
      }];
}

// Animates consecutive circles of the progress bar appearing.
- (void)animateProgressBarLoading {
  __weak __typeof(self) weakSelf = self;
  for (NSUInteger i = 0; i < kProgressBarCirclesAmount; i++) {
    [UIView animateWithDuration:0
                          delay:(kProgressBarLoadingDuration /
                                 kProgressBarCirclesAmount) *
                                i
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                       [weakSelf setProgressBarSubviewsAlphaAtIndex:i];
                     }
                     completion:nil];
  }
}

// Helper for starting lock appearing animation.
- (void)startLockAppearingAnimation {
  [_lockAppearingAnimation startAnimation];
}

// Helper for making lock image visible.
- (void)showLockImage {
  _lockImage.hidden = NO;
}

// Helper for starting progress bar loading animation.
- (void)startProgressBarLoadingAnimation {
  [_progressBarLoadingAnimation startAnimation];
}

// Helper for starting images sliding in animation.
- (void)startImagesSlidingInAnimation {
  [_imagesSlidingInAnimation startAnimation];
}

// Helper for starting favicon appearing animation.
- (void)startFaviconAppearingAnimation {
  [_faviconAppearingAnimation startAnimationAfterDelay:kFaviconAppearingDelay];
}

// Helper for animating profile images sliding in.
- (void)animateImagesSlidingIn {
  _progressBarView.hidden = YES;
  [self sendRecipientImageToBack];
  [self setImagesCenterXConstraint:kImagesSlidedInCenterXConstant];
}

// Helper for making favicon container visible.
- (void)showFaviconContainer {
  _faviconContainerView.hidden = NO;
}

// Helper for displaying success status and notifying delegate when favicon
// appearing animation completes.
- (void)onFaviconAppearingAnimationCompleted {
  [self displaySuccessStatus];
  [self.delegate startPasswordSharing];
}

// Helper for animating sharing cancelled.
- (void)animateSharingCancelled {
  _progressBarView.hidden = YES;
  [self sendRecipientImageToBack];
  [self setImagesCenterXConstraint:0];
}

// Helper for animating displaying success status.
- (void)animateDisplayingSuccessStatus {
  _cancelButton.alpha = 0.0;
  _subtitleTextView.alpha = 1.0;
  _footerTextView.alpha = 1.0;
  _doneButton.alpha = 1.0;
}

// Helper for animating displaying cancelled status.
- (void)animateDisplayingCancelledStatus {
  _cancelButton.alpha = 0.0;
  _doneButton.alpha = 1.0;
}

// Helper called when status transition animation completes.
- (void)onStatusTransitionCompleted {
  _cancelButton.hidden = YES;
}

// Orchestrates sheet detent expansion and status view transition animations.
- (void)animateStatusTransitionWithAnimations:(void (^)(void))animations {
  __weak __typeof(self) weakSelf = self;
  [self.sheetPresentationController animateChanges:^{
    [weakSelf recalculatePreferredHeightDetentAndLayout];
  }];
  [UIView animateWithDuration:kStatusTransitionDuration
      animations:animations
      completion:^(BOOL finished) {
        [weakSelf onStatusTransitionCompleted];
      }];
}

// Helper for setting alpha of progress bar circle at `index` to 1.
- (void)setProgressBarSubviewsAlphaAtIndex:(NSUInteger)index {
  if (index < _progressBarView.subviews.count) {
    _progressBarView.subviews[index].alpha = 1.0;
  }
}

// Moves the recipient image to the back so that it's below the sender image
// when they overlap.
- (void)sendRecipientImageToBack {
  [_animationView sendSubviewToBack:_recipientImageView];
}

// Sets constant for sender and recipients centerX constraint so that the sender
// is on the left from the middle of the view and the recipients on the right.
- (void)setImagesCenterXConstraint:(CGFloat)constant {
  _senderImageCenterXConstraint.constant = -constant;
  _recipientImageCenterXConstraint.constant = constant;
  [self.view layoutIfNeeded];
}

// Calculates and sets detent based on the height of content.
- (void)recalculatePreferredHeightDetent {
  self.sheetPresentationController.detents = @[
    [self preferredHeightDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
}

// Recalculates preferred height detent and lays out view inside animated block.
- (void)recalculatePreferredHeightDetentAndLayout {
  [self recalculatePreferredHeightDetent];
  [self.view layoutIfNeeded];
}

// Creates a UITextView with subtitle and footer defaults.
- (UITextView*)createTextView {
  UITextView* view = [[UITextView alloc] init];
  view.textAlignment = NSTextAlignmentCenter;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  view.adjustsFontForContentSizeCategory = YES;
  view.delegate = self;
  view.editable = NO;
  view.selectable = YES;
  view.scrollEnabled = NO;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  return view;
}

// Adds link attribute to the specified `range` of the `view`.
- (void)addLinkAttributeToTextView:(UITextView*)view range:(NSRange)range {
  NSMutableAttributedString* linkText = [[NSMutableAttributedString alloc]
      initWithAttributedString:view.attributedText];
  NSDictionary* linkAttributes = @{
    NSLinkAttributeName : @"",
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle)
  };
  [linkText addAttributes:linkAttributes range:range];
  view.attributedText = linkText;
}

// Adds bold attribute to the specified `range` of the `view`.
- (void)addBoldAttributeToTextView:(UITextView*)view range:(NSRange)range {
  NSMutableAttributedString* boldText = [[NSMutableAttributedString alloc]
      initWithAttributedString:view.attributedText];
  UIFontDescriptor* boldDescriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleBody]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  [boldText addAttribute:NSFontAttributeName
                   value:[UIFont fontWithDescriptor:boldDescriptor size:0.0]
                   range:range];
  view.attributedText = boldText;
}

// Helper to create the subtitle.
- (UITextView*)createSubtitle {
  UITextView* subtitle = [self createTextView];
  subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  subtitle.textColor = [UIColor colorNamed:kTextPrimaryColor];

  StringWithTags stringWithBolds =
      ParseStringWithTags(_subtitleString, kBeginBoldTag, kEndBoldTag);
  subtitle.text = stringWithBolds.string;

  for (const NSRange& range : stringWithBolds.ranges) {
    [self addBoldAttributeToTextView:subtitle range:range];
  }

  return subtitle;
}

// Helper to create the footer.
- (UITextView*)createFooter {
  UITextView* footer = [self createTextView];
  footer.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  footer.textColor = [UIColor colorNamed:kTextSecondaryColor];
  footer.accessibilityIdentifier = kSharingStatusFooterId;

  StringWithTags stringWithTags = ParseStringWithLinks(_footerString);
  footer.text = stringWithTags.string;
  if (!stringWithTags.ranges.empty()) {
    [self addLinkAttributeToTextView:footer range:stringWithTags.ranges[0]];
  }

  return footer;
}

// Helper for creating the done button.
- (UIButton*)createDoneButton {
  ChromeButton* doneButton =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  [doneButton addTarget:self
                 action:@selector(doneButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  doneButton.title = l10n_util::GetNSString(IDS_DONE);
  doneButton.accessibilityIdentifier = kSharingStatusDoneButtonID;
  return doneButton;
}

// Creates done button with 0 alpha, adds it to the view and sets its
// constraints.
- (void)addDoneButtonWithBottomPadding {
  UIView* view = self.view;
  _doneButton = [self createDoneButton];
  _doneButton.alpha = 0.0;
  [view addSubview:_doneButton];

  [NSLayoutConstraint activateConstraints:@[
    [_doneButton.leadingAnchor constraintEqualToAnchor:view.leadingAnchor
                                              constant:kHorizontalPadding],
    [_doneButton.trailingAnchor constraintEqualToAnchor:view.trailingAnchor
                                               constant:-kHorizontalPadding],
    [_doneButton.topAnchor
        constraintGreaterThanOrEqualToAnchor:_stackView.bottomAnchor
                                    constant:kVerticalSpacing],
    [_doneButton.bottomAnchor constraintEqualToAnchor:view.bottomAnchor
                                             constant:-kBottomPadding],
  ]];
}

// Replaces text of the title label, hides cancel button, and makes subtitle,
// footer, and done button visible while smoothly expanding the sheet detent.
- (void)displaySuccessStatus {
  if (_doneButton) {
    return;
  }

  _titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_SUCCESS_TITLE);
  _cancelButton.userInteractionEnabled = NO;

  _subtitleTextView = [self createSubtitle];
  _subtitleTextView.alpha = 0.0;
  [_stackView addArrangedSubview:_subtitleTextView];

  _footerTextView = [self createFooter];
  _footerTextView.alpha = 0.0;
  [_stackView addArrangedSubview:_footerTextView];

  [self addDoneButtonWithBottomPadding];

  __weak __typeof(self) weakSelf = self;
  [self animateStatusTransitionWithAnimations:^{
    [weakSelf animateDisplayingSuccessStatus];
  }];

  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  _titleLabel);
}

// Replaces text of the title label, hides cancel button, and makes done button
// visible while smoothly expanding the sheet detent.
- (void)displayCancelledStatus {
  if (_doneButton) {
    return;
  }

  _titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_CANCELLED_TITLE);
  _cancelButton.userInteractionEnabled = NO;

  [self addDoneButtonWithBottomPadding];

  __weak __typeof(self) weakSelf = self;
  [self animateStatusTransitionWithAnimations:^{
    [weakSelf animateDisplayingCancelledStatus];
  }];

  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  _titleLabel);
}

// Stops any ongoing animations and starts a new one (profile images sliding to
// the middle).
- (void)cancelButtonTapped {
  [_imagesSlidingOutAnimation stopAnimation:YES];
  [_progressBarLoadingAnimation stopAnimation:YES];
  [_imagesSlidingInAnimation stopAnimation:YES];
  [_faviconAppearingAnimation stopAnimation:YES];

  [_sharingCancelledAnimation startAnimation];

  LogPasswordSharingInteraction(
      PasswordSharingInteraction::kSharingConfirmationCancelClicked);
}

// Handles done buttons clicks by dismissing the view.
- (void)doneButtonTapped {
  [self.delegate sharingStatusWasDismissed:self];
}

@end
