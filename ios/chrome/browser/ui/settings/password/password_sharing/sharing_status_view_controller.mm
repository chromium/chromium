// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_view_controller_presentation_delegate.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/button_util.h"
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

// Container view for the animation.
@property(nonatomic, strong) UIView* animationView;

// Profile image of the sender.
@property(nonatomic, strong) UIImageView* senderImageView;
@property(nonatomic, strong) UIImage* senderImage;

// Profile image of the recipient (or merged avatar of multiple recipients).
@property(nonatomic, strong) UIImageView* recipientImageView;
@property(nonatomic, strong) UIImage* recipientImage;

// Lock image displayed in the animation.
@property(nonatomic, strong) UIImageView* lockImage;

// Rectangle view with fixed length and height containing fixed amount of
// circles.
@property(nonatomic, strong) UIView* progressBarView;

// The container for the favicon view that is displayed below the recipient and
// sender images in successful status view.
@property(nonatomic, strong) FaviconContainerView* faviconContainerView;

// Stack view containing animation container view, title, subtitle and footer.
@property(nonatomic, strong) UIStackView* stackView;

// Animates profile image of the sender sliding to the left and profile images
// of recipients sliding to the right.
@property(nonatomic, strong) UIViewPropertyAnimator* imagesSlidingOutAnimation;

// Animates lock appearing in the middle between profile images.
@property(nonatomic, strong) UIViewPropertyAnimator* lockAppearingAnimation;

// Animates the progress bar going from the left to right image.
@property(nonatomic, strong)
    UIViewPropertyAnimator* progressBarLoadingAnimation;

// Animates progress bar and lock disappearing and profile images sliding to the
// middle.
@property(nonatomic, strong) UIViewPropertyAnimator* imagesSlidingInAnimation;

// Animates favicon appearing below recipient and sender image.
@property(nonatomic, strong) UIViewPropertyAnimator* faviconAppearingAnimation;

// Animates profile images sliding to the middle on cancel button tap.
@property(nonatomic, strong) UIViewPropertyAnimator* sharingCancelledAnimation;

// Contains the information that sharing is in progress at first and then is
// modified to convey the result status.
@property(nonatomic, strong) UILabel* titleLabel;

// Subtitle string that will be displayed when the sharing is succesful.
@property(nonatomic, strong) NSString* subtitleString;

// Footer string that will be displayed when the sharing is succesful.
@property(nonatomic, strong) NSString* footerString;

// The button that cancels the sharing process.
@property(nonatomic, strong) UIButton* cancelButton;

// Url of the site for which the password is being shared.
@property(nonatomic, readonly) GURL URL;

@end

@implementation SharingStatusViewController {
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
  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    [self createAnimationContainerView], [self createTitleLabel]
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = kVerticalSpacing;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  self.stackView = verticalStack;
  [view addSubview:verticalStack];

  // Add cancel button below the stack.
  UIButton* cancelButton = [self createCancelButton];
  [view addSubview:cancelButton];

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
    [cancelButton.topAnchor
        constraintGreaterThanOrEqualToAnchor:verticalStack.bottomAnchor
                                    constant:kVerticalSpacing],
    [cancelButton.bottomAnchor constraintEqualToAnchor:view.bottomAnchor
                                              constant:-kBottomPadding],
    [cancelButton.centerXAnchor
        constraintEqualToAnchor:verticalStack.centerXAnchor],
  ]];

  [self createAnimations];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Make sure that the title is focused when the view appears.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.titleLabel);
  [self.imagesSlidingOutAnimation
      startAnimationAfterDelay:kImagesSlidingOutDelay];
}

- (void)viewDidDisappear:(BOOL)animated {
  // Stop the ongoing animations so that their completion is not called.
  [self.imagesSlidingOutAnimation stopAnimation:YES];
  [self.lockAppearingAnimation stopAnimation:YES];
  [self.progressBarLoadingAnimation stopAnimation:YES];
  [self.imagesSlidingInAnimation stopAnimation:YES];
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
  _subtitleString = subtitleString;
}

- (void)setFooterString:(NSString*)footerString {
  _footerString = footerString;
}

- (void)setURL:(const GURL&)URL {
  _URL = URL;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.delegate changePasswordLinkWasTapped];
  return NO;
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
      [[UIImageView alloc] initWithImage:self.senderImage];
  senderImageView.translatesAutoresizingMaskIntoConstraints = NO;
  self.senderImageView = senderImageView;
  return senderImageView;
}

// Helper for creating recipient image view.
- (UIImageView*)createRecipientImageView {
  UIImageView* recipientImageView =
      [[UIImageView alloc] initWithImage:self.recipientImage];
  recipientImageView.translatesAutoresizingMaskIntoConstraints = NO;
  recipientImageView.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  self.recipientImageView = recipientImageView;
  return recipientImageView;
}

// Helper for creating progress bar view.
- (UIView*)createProgressBarView {
  UIView* progressBarView = [[UIView alloc] init];
  progressBarView.translatesAutoresizingMaskIntoConstraints = NO;
  progressBarView.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  self.progressBarView = progressBarView;
  return progressBarView;
}

// Helper for creating the lock image view.
- (UIImageView*)createLockImage {
  UIImageView* lockImage = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kLockSymbol,
                                               kLockSymbolPointSize)];
  lockImage.translatesAutoresizingMaskIntoConstraints = NO;
  lockImage.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  lockImage.hidden = YES;
  self.lockImage = lockImage;
  return lockImage;
}

// Creates `kProgressBarCirclesAmount` blue circles in the progress bar view.
- (void)createProgressBarSubviews {
  UIView* progressBarView = self.progressBarView;
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
    [progressBarView addSubview:circleView];
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
             completion:^(FaviconAttributes* attributes) {
               [faviconView configureWithAttributes:attributes];
             }];

  return faviconView;
}

// Creates and returns the container for the favicon view.
- (FaviconContainerView*)createFaviconContainerView {
  FaviconContainerView* faviconContainerView =
      [[FaviconContainerView alloc] init];
  faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [faviconContainerView
      setFaviconBackgroundColor:[UIColor colorNamed:kPrimaryBackgroundColor]];
  faviconContainerView.hidden = YES;
  self.faviconContainerView = faviconContainerView;
  return faviconContainerView;
}

// Creates the container view for the animation.
- (UIView*)createAnimationContainerView {
  UIView* animationView = [[UIView alloc] init];
  animationView = [[UIView alloc] init];
  animationView.translatesAutoresizingMaskIntoConstraints = NO;

  // Add progress bar view.
  UIView* progressBarView = [self createProgressBarView];
  [animationView addSubview:progressBarView];

  // Add progress bar circles.
  [self createProgressBarSubviews];

  // Add lock image.
  UIImageView* lockImage = [self createLockImage];
  [progressBarView addSubview:lockImage];

  // Add sender profile image.
  UIImageView* senderImageView = [self createSenderImageView];
  [animationView addSubview:senderImageView];

  // Add recipient profile image.
  UIImageView* recipientImageView = [self createRecipientImageView];
  [animationView addSubview:recipientImageView];

  // Add favicon and its container.
  FaviconContainerView* faviconContainerView =
      [self createFaviconContainerView];
  [animationView addSubview:faviconContainerView];
  FaviconView* faviconView = [self createFaviconView];
  [faviconContainerView addSubview:faviconView];

  [NSLayoutConstraint activateConstraints:@[
    // Sender image constraints.
    [senderImageView.topAnchor constraintEqualToAnchor:animationView.topAnchor
                                              constant:kVerticalSpacing],
    [senderImageView.bottomAnchor
        constraintEqualToAnchor:animationView.bottomAnchor
                       constant:-kVerticalSpacing],
    [senderImageView.widthAnchor constraintEqualToConstant:kProfileImageSize],
    [senderImageView.heightAnchor constraintEqualToConstant:kProfileImageSize],

    // Recipient image constraints.
    [recipientImageView.centerYAnchor
        constraintEqualToAnchor:senderImageView.centerYAnchor],
    [recipientImageView.widthAnchor
        constraintEqualToConstant:kProfileImageSize],
    [recipientImageView.heightAnchor
        constraintEqualToConstant:kProfileImageSize],

    // Progress bar constraints.
    [progressBarView.centerXAnchor
        constraintEqualToAnchor:animationView.centerXAnchor],
    [progressBarView.centerYAnchor
        constraintEqualToAnchor:senderImageView.centerYAnchor],
    [progressBarView.widthAnchor constraintEqualToConstant:kProgressBarWidth],
    [progressBarView.heightAnchor constraintEqualToConstant:kProgressBarHeight],

    // Lock image constraints.
    [lockImage.centerYAnchor
        constraintEqualToAnchor:senderImageView.centerYAnchor],
    [lockImage.centerXAnchor
        constraintEqualToAnchor:animationView.centerXAnchor],

    // Favicon constraints.
    [faviconContainerView.topAnchor
        constraintEqualToAnchor:senderImageView.bottomAnchor
                       constant:-kFaviconProfileImageVerticalOverlap],
    [faviconContainerView.centerXAnchor
        constraintEqualToAnchor:animationView.centerXAnchor],
    [faviconContainerView.widthAnchor
        constraintEqualToConstant:kFaviconContainerSize],
    [faviconContainerView.heightAnchor
        constraintEqualToConstant:kFaviconContainerSize],
    [faviconView.centerXAnchor
        constraintEqualToAnchor:faviconContainerView.centerXAnchor],
    [faviconView.centerYAnchor
        constraintEqualToAnchor:faviconContainerView.centerYAnchor],
    [faviconView.widthAnchor constraintEqualToConstant:kFaviconSize],
    [faviconView.heightAnchor constraintEqualToConstant:kFaviconSize],
  ]];

  _senderImageCenterXConstraint = [senderImageView.centerXAnchor
      constraintEqualToAnchor:animationView.centerXAnchor];
  _senderImageCenterXConstraint.active = YES;
  _recipientImageCenterXConstraint = [recipientImageView.centerXAnchor
      constraintEqualToAnchor:animationView.centerXAnchor];
  _recipientImageCenterXConstraint.active = YES;

  self.animationView = animationView;
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
  self.titleLabel = title;
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
  self.cancelButton = cancelButton;
  return cancelButton;
}

// Creates sharing status animations that are started one by one.
- (void)createAnimations {
  UICubicTimingParameters* imagesSlidingTimingParams =
      [[UICubicTimingParameters alloc]
          initWithControlPoint1:CGPointMake(0.7, 0.0)
                  controlPoint2:CGPointMake(0.45, 1.45)];
  self.imagesSlidingOutAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kImagesSlidingOutDuration
      timingParameters:imagesSlidingTimingParams];
  __weak __typeof(self) weakSelf = self;
  [self.imagesSlidingOutAnimation addAnimations:^{
    [weakSelf setImagesCenterXConstraint:kImagesSlidedOutCenterXConstant];
  }];
  [self.imagesSlidingOutAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf.lockAppearingAnimation startAnimation];
      }];

  self.lockAppearingAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kLockAppearingDuration
                 curve:UIViewAnimationCurveEaseIn
            animations:^{
              weakSelf.lockImage.hidden = NO;
            }];
  [self.lockAppearingAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf.progressBarLoadingAnimation startAnimation];
      }];

  self.progressBarLoadingAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kProgressBarLoadingDuration
                 curve:UIViewAnimationCurveLinear
            animations:^{
              [weakSelf animateProgressBarLoading];
            }];
  [self.progressBarLoadingAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf.imagesSlidingInAnimation startAnimation];
      }];

  self.imagesSlidingInAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kImagesSlidingInDuration
      timingParameters:imagesSlidingTimingParams];
  [self.imagesSlidingInAnimation addAnimations:^{
    weakSelf.progressBarView.hidden = YES;
    [weakSelf sendRecipientImageToBack];
    [weakSelf setImagesCenterXConstraint:kImagesSlidedInCenterXConstant];
  }];
  [self.imagesSlidingInAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf.faviconAppearingAnimation
            startAnimationAfterDelay:kFaviconAppearingDelay];
      }];

  self.faviconAppearingAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kFaviconAppearingDuration
                 curve:UIViewAnimationCurveEaseIn
            animations:^{
              weakSelf.faviconContainerView.hidden = NO;
            }];
  __weak __typeof(self.delegate) weakDelegate = self.delegate;
  [self.faviconAppearingAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf displaySuccessStatus];
        [weakDelegate startPasswordSharing];
      }];

  UICubicTimingParameters* animationCancelledTimingParams =
      [[UICubicTimingParameters alloc]
          initWithControlPoint1:CGPointMake(0.7, -0.45)
                  controlPoint2:CGPointMake(0.45, 1.0)];
  self.sharingCancelledAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kSharingCancelledDuration
      timingParameters:animationCancelledTimingParams];
  [self.sharingCancelledAnimation addAnimations:^{
    weakSelf.progressBarView.hidden = YES;
    [weakSelf sendRecipientImageToBack];
    [weakSelf setImagesCenterXConstraint:0];
  }];
  [self.sharingCancelledAnimation
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
                       weakSelf.progressBarView.subviews[i].alpha = 1.0;
                     }
                     completion:nil];
  }
}

// Moves the recipient image to the back so that it's below the sender image
// when they overlap.
- (void)sendRecipientImageToBack {
  [self.animationView sendSubviewToBack:self.recipientImageView];
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
    UISheetPresentationControllerDetent.largeDetent
  ];
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
      ParseStringWithTags(self.subtitleString, kBeginBoldTag, kEndBoldTag);
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

  StringWithTags stringWithTags = ParseStringWithLinks(self.footerString);
  footer.text = stringWithTags.string;
  if (!stringWithTags.ranges.empty()) {
    [self addLinkAttributeToTextView:footer range:stringWithTags.ranges[0]];
  }

  return footer;
}

// Helper for creating the done button.
- (UIButton*)createDoneButton {
  UIButton* doneButton = PrimaryActionButton(YES);
  [doneButton addTarget:self
                 action:@selector(doneButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  SetConfigurationTitle(doneButton, l10n_util::GetNSString(IDS_DONE));
  doneButton.accessibilityIdentifier = kSharingStatusDoneButtonID;
  return doneButton;
}

// Creates done button, adds it to the view and sets its constraints.
- (void)addDoneButtonWithBottomPadding {
  UIView* view = self.view;
  UIButton* doneButton = [self createDoneButton];
  [view addSubview:doneButton];

  [NSLayoutConstraint activateConstraints:@[
    [doneButton.leadingAnchor constraintEqualToAnchor:view.leadingAnchor
                                             constant:kHorizontalPadding],
    [doneButton.trailingAnchor constraintEqualToAnchor:view.trailingAnchor
                                              constant:-kHorizontalPadding],
    [doneButton.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.stackView.bottomAnchor
                                    constant:kVerticalSpacing],
    [doneButton.bottomAnchor constraintEqualToAnchor:view.bottomAnchor
                                            constant:-kBottomPadding],
  ]];
}

// Replaces text of the title label, cancel button with done button and adds a
// subtitle and a footer.
- (void)displaySuccessStatus {
  self.titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_SUCCESS_TITLE);
  self.cancelButton.hidden = YES;

  UIStackView* stackView = self.stackView;
  [stackView addArrangedSubview:[self createSubtitle]];
  [stackView addArrangedSubview:[self createFooter]];

  [self addDoneButtonWithBottomPadding];
  [self recalculatePreferredHeightDetent];
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  self.titleLabel);
}

// Replaces text of the title label and adds a done button.
// TODO(crbug.com/40275395): Add test.
- (void)displayCancelledStatus {
  self.titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_CANCELLED_TITLE);
  self.cancelButton.hidden = YES;

  [self addDoneButtonWithBottomPadding];
  [self recalculatePreferredHeightDetent];
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  self.titleLabel);
}

// Stops any ongoing animations and starts a new one (profile images sliding to
// the middle).
- (void)cancelButtonTapped {
  [self.imagesSlidingOutAnimation stopAnimation:YES];
  [self.progressBarLoadingAnimation stopAnimation:YES];
  [self.imagesSlidingInAnimation stopAnimation:YES];
  [self.faviconAppearingAnimation stopAnimation:YES];

  [self.sharingCancelledAnimation startAnimation];

  LogPasswordSharingInteraction(
      PasswordSharingInteraction::kSharingConfirmationCancelClicked);
}

// Handles done buttons clicks by dismissing the view.
- (void)doneButtonTapped {
  [self.delegate sharingStatusWasDismissed:self];
}

@end
