// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/crurl.h"
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
const CGFloat kProfileImageSize = 60.0;
const CGFloat kLockSymbolPointSize = 22.0;
const CGFloat kFaviconContainerSize = 30.0;
const CGFloat kFaviconSize = 22.0;

// Spacing and padding constraints.
const CGFloat kVerticalSpacing = 16.0;
const CGFloat kTopPadding = 20.0;
const CGFloat kBottomPadding = 42.0;
const CGFloat kHorizontalPadding = 16.0;
const CGFloat kTitleDoneButtonSpacing = 48.0;
const CGFloat kFaviconProfileImageVerticalOverlap = 10.0;

// Durations of specific parts of the animation in seconds.
const CGFloat kImagesSlidingOutDuration = 1.0;
const CGFloat kProgressBarLoadingDuration = 3.25;
const CGFloat kImagesSlidingInDuration = 1.0;
const CGFloat kFaviconAppearingDuration = 0.15;
const CGFloat kFaviconAppearingDelay = 0.1;
const CGFloat kSharingCancelledDuration = 0.5;

// Distance by which the profile images need to be moved when sliding.
const CGFloat kImagesSlidingOutDistance = 78;
const CGFloat kImagesSlidingInDistance = 51;

// Tags marking parts of string that should have a bold font.
NSString* const kBeginBoldTag = @"BEGIN_BOLD[ \t]*";
NSString* const kEndBoldTag = @"[ \t]*END_BOLD";

// Accessibility identifiers of text views with links.
NSString* const kSharingStatusFooterId = @"SharingStatusViewFooter";
NSString* const kSharingStatusSubtitleId = @"SharingStatusViewSubtitle";

}  // namespace

@interface SharingStatusViewController () <UITextViewDelegate>

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

// Animates profile image of the sender sliding to the left and profile images
// of recipients sliding to the right.
@property(nonatomic, strong) UIViewPropertyAnimator* imagesSlidingOutAnimation;

// Animates lock appearing in the middle between profile images and the progress
// bar going from the left to right.
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

@implementation SharingStatusViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* view = self.view;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  // Add container view for the animation.
  UIView* animationView = [[UIView alloc] init];
  animationView = [[UIView alloc] init];
  animationView.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:animationView];

  // Add sender profile image.
  UIImageView* senderImageView = [self createSenderImageView];
  [animationView addSubview:senderImageView];

  // Add recipient profile image.
  UIImageView* recipientImageView = [self createRecipientImageView];
  [animationView insertSubview:recipientImageView belowSubview:senderImageView];

  // Add progress bar view.
  UIView* progressBarView = [self createProgressBarView];
  [animationView insertSubview:progressBarView belowSubview:recipientImageView];

  // Add progress bar circles.
  [self createProgressBarSubviews];

  // Add lock image.
  UIImageView* lockImage = [self createLockImage];
  [progressBarView addSubview:lockImage];

  // Add favicon and its container.
  FaviconContainerView* faviconContainerView =
      [self createFaviconContainerView];
  [animationView insertSubview:faviconContainerView
                  aboveSubview:senderImageView];
  FaviconView* faviconView = [self createFaviconView];
  [faviconContainerView addSubview:faviconView];

  // Add title label.
  UILabel* titleLabel = [self createTitleLabel];
  [view addSubview:titleLabel];

  // Add cancel button below the label.
  UIButton* cancelButton = [self createCancelButton];
  [view addSubview:cancelButton];

  [NSLayoutConstraint activateConstraints:@[
    // Animation container constraints.
    [animationView.topAnchor constraintEqualToAnchor:view.topAnchor
                                            constant:kTopPadding],
    [animationView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor
                                                constant:kHorizontalPadding],
    [animationView.trailingAnchor constraintEqualToAnchor:view.trailingAnchor
                                                 constant:-kHorizontalPadding],
    [animationView.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],

    // Sender image constraints.
    [senderImageView.topAnchor constraintEqualToAnchor:animationView.topAnchor
                                              constant:kVerticalSpacing],
    [senderImageView.bottomAnchor
        constraintEqualToAnchor:animationView.bottomAnchor
                       constant:-kVerticalSpacing],
    [senderImageView.centerXAnchor
        constraintEqualToAnchor:animationView.centerXAnchor],

    // Recipient image constraints.
    [recipientImageView.centerYAnchor
        constraintEqualToAnchor:senderImageView.centerYAnchor],
    [recipientImageView.centerXAnchor
        constraintEqualToAnchor:senderImageView.centerXAnchor],

    // Progress bar constraints.
    [progressBarView.centerXAnchor
        constraintEqualToAnchor:senderImageView.centerXAnchor],
    [progressBarView.centerYAnchor
        constraintEqualToAnchor:senderImageView.centerYAnchor],
    [progressBarView.widthAnchor constraintEqualToConstant:kProgressBarWidth],
    [progressBarView.heightAnchor constraintEqualToConstant:kProgressBarHeight],

    // Lock image constraints.
    [lockImage.centerYAnchor
        constraintEqualToAnchor:senderImageView.centerYAnchor],
    [lockImage.centerXAnchor
        constraintEqualToAnchor:senderImageView.centerXAnchor],

    // Favicon constraints.
    [faviconContainerView.topAnchor
        constraintEqualToAnchor:senderImageView.bottomAnchor
                       constant:-kFaviconProfileImageVerticalOverlap],
    [faviconContainerView.centerXAnchor
        constraintEqualToAnchor:senderImageView.centerXAnchor],
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

    // Title constraints.
    [titleLabel.topAnchor constraintEqualToAnchor:animationView.bottomAnchor
                                         constant:kVerticalSpacing],
    [titleLabel.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],

    // Cancel button constraints.
    [cancelButton.topAnchor constraintEqualToAnchor:titleLabel.bottomAnchor
                                           constant:kVerticalSpacing],
    [cancelButton.bottomAnchor constraintEqualToAnchor:view.bottomAnchor
                                              constant:-kBottomPadding],
    [cancelButton.centerXAnchor
        constraintEqualToAnchor:animationView.centerXAnchor],
  ]];

  [self createAnimations];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  [self.imagesSlidingOutAnimation startAnimation];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self.imagesSlidingOutAnimation stopAnimation:YES];
  [self.progressBarLoadingAnimation stopAnimation:YES];
  [self.imagesSlidingInAnimation stopAnimation:YES];
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
  if (textView.accessibilityIdentifier == kSharingStatusSubtitleId) {
    [self.delegate learnMoreLinkWasTapped];
  } else if (textView.accessibilityIdentifier == kSharingStatusFooterId) {
    [self.delegate changePasswordLinkWasTapped];
  }
  return NO;
}

#pragma mark - Private

// Helper for creating sender image view.
- (UIImageView*)createSenderImageView {
  UIImageView* senderImageView =
      [[UIImageView alloc] initWithImage:self.senderImage];
  senderImageView.translatesAutoresizingMaskIntoConstraints = NO;
  senderImageView.hidden = YES;
  self.senderImageView = senderImageView;
  return senderImageView;
}

// Helper for creating recipient image view.
- (UIImageView*)createRecipientImageView {
  UIImageView* recipientImageView = [[UIImageView alloc]
      initWithImage:CircularImageFromImage(self.recipientImage,
                                           kProfileImageSize)];
  recipientImageView.translatesAutoresizingMaskIntoConstraints = NO;
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
  UIImageView* senderImageView = self.senderImageView;
  UIImageView* recipientImageView = self.recipientImageView;
  UIImageView* lockImage = self.lockImage;
  UIView* progressBarView = self.progressBarView;

  self.imagesSlidingOutAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kImagesSlidingOutDuration
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              senderImageView.hidden = NO;
              senderImageView.center = CGPointMake(
                  senderImageView.center.x - kImagesSlidingOutDistance,
                  senderImageView.center.y);
              recipientImageView.center = CGPointMake(
                  recipientImageView.center.x + kImagesSlidingOutDistance,
                  recipientImageView.center.y);
            }];

  __weak __typeof(self) weakSelf = self;
  [self.imagesSlidingOutAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf.progressBarLoadingAnimation startAnimation];
      }];

  self.progressBarLoadingAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kProgressBarLoadingDuration
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              lockImage.hidden = NO;

              for (NSInteger i = 0; i < kProgressBarCirclesAmount; i++) {
                [UIView animateWithDuration:0
                                      delay:(kProgressBarLoadingDuration /
                                             kProgressBarCirclesAmount) *
                                            i
                                    options:UIViewAnimationOptionCurveEaseInOut
                                 animations:^{
                                   progressBarView.subviews[i].alpha = 1.0;
                                 }
                                 completion:nil];
              }
            }];
  [self.progressBarLoadingAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf.imagesSlidingInAnimation startAnimation];
      }];

  self.imagesSlidingInAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kImagesSlidingInDuration
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              lockImage.hidden = YES;
              progressBarView.hidden = YES;
              senderImageView.center = CGPointMake(
                  senderImageView.center.x + kImagesSlidingInDistance,
                  senderImageView.center.y);
              recipientImageView.center = CGPointMake(
                  recipientImageView.center.x - kImagesSlidingInDistance,
                  recipientImageView.center.y);
            }];
  [self.imagesSlidingInAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf.faviconAppearingAnimation
            startAnimationAfterDelay:kFaviconAppearingDelay];
      }];

  self.faviconAppearingAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kFaviconAppearingDuration
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              self.faviconContainerView.hidden = NO;
            }];
  __weak __typeof(self.delegate) weakDelegate = self.delegate;
  [self.faviconAppearingAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf displaySuccessStatus];
        [weakDelegate startPasswordSharing];
      }];

  self.sharingCancelledAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kSharingCancelledDuration
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              lockImage.hidden = YES;
              progressBarView.hidden = YES;
              senderImageView.center = CGPointMake(progressBarView.center.x,
                                                   senderImageView.center.y);
              recipientImageView.center = CGPointMake(
                  progressBarView.center.x, recipientImageView.center.y);
            }];
  [self.sharingCancelledAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf displayCancelledStatus];
      }];
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
  [linkText addAttribute:NSLinkAttributeName value:@"" range:range];
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
  subtitle.accessibilityIdentifier = kSharingStatusSubtitleId;

  StringWithTags stringWithBolds =
      ParseStringWithTags(self.subtitleString, kBeginBoldTag, kEndBoldTag);
  StringWithTags stringWithLinks = ParseStringWithLinks(stringWithBolds.string);
  subtitle.text = stringWithLinks.string;

  for (const NSRange& range : stringWithBolds.ranges) {
    [self addBoldAttributeToTextView:subtitle range:range];
  }
  [self addLinkAttributeToTextView:subtitle range:stringWithLinks.ranges[0]];

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
  doneButton.accessibilityIdentifier = kSharingStatusDoneButtonId;
  return doneButton;
}

// Replaces text of the title label, cancel button with done button and adds a
// subtitle and a footer.
- (void)displaySuccessStatus {
  UILabel* titleLabel = self.titleLabel;
  titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_SUCCESS_TITLE);
  self.cancelButton.hidden = YES;

  UIView* view = self.view;
  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    [self createSubtitle], [self createFooter], [self createDoneButton]
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = kVerticalSpacing;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:verticalStack];

  [NSLayoutConstraint activateConstraints:@[
    [verticalStack.topAnchor constraintEqualToAnchor:titleLabel.bottomAnchor
                                            constant:kVerticalSpacing],
    [verticalStack.leadingAnchor constraintEqualToAnchor:view.leadingAnchor
                                                constant:kHorizontalPadding],
    [verticalStack.trailingAnchor constraintEqualToAnchor:view.trailingAnchor
                                                 constant:-kHorizontalPadding],
    [verticalStack.bottomAnchor constraintEqualToAnchor:view.bottomAnchor
                                               constant:-kBottomPadding],
  ]];

  [view setNeedsLayout];
  [view layoutIfNeeded];
}

// Replaces text of the title label and adds a done button.
// TODO(crbug.com/1463882): Add test.
- (void)displayCancelledStatus {
  UILabel* titleLabel = self.titleLabel;
  titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_CANCELLED_TITLE);
  self.cancelButton.hidden = YES;

  UIView* view = self.view;
  UIButton* doneButton = [self createDoneButton];
  [view addSubview:doneButton];

  [NSLayoutConstraint activateConstraints:@[
    // Constraints for the done button.
    [doneButton.topAnchor constraintEqualToAnchor:titleLabel.bottomAnchor
                                         constant:kTitleDoneButtonSpacing],
    [doneButton.bottomAnchor constraintEqualToAnchor:view.bottomAnchor
                                            constant:-kBottomPadding],
    [doneButton.leadingAnchor constraintEqualToAnchor:view.leadingAnchor
                                             constant:kHorizontalPadding],
    [doneButton.trailingAnchor constraintEqualToAnchor:view.trailingAnchor
                                              constant:-kHorizontalPadding],
    [doneButton.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],
  ]];

  [view setNeedsLayout];
  [view layoutIfNeeded];
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
