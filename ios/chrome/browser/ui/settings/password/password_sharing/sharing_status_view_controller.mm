// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_view_controller_presentation_delegate.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Progress bar dimensions.
const CGFloat kProgressBarWidth = 100.0;
const CGFloat kProgressBarHeight = 30.0;
const CGFloat kProgressBarCircleDiameter = 3.0;
const CGFloat kProgressBarCircleSpacing = 2.0;
const NSInteger kProgressBarCirclesAmount = 20;

// Loaded images size dimensions.
const CGFloat kProfileImageSize = 60.0;
const CGFloat kShieldLockSize = 30.0;

// Spacing and padding constraints.
const CGFloat kVerticalSpacing = 16.0;
const CGFloat kTopPadding = 20.0;
const CGFloat kBottomPadding = 42.0;
const CGFloat kHorizontalPadding = 16.0;
const CGFloat kTitleDoneButtonSpacing = 48.0;

// Durations of specific parts of the animation.
const CGFloat kImagesSlidingOutDuration = 1.0;
const CGFloat kProgressBarLoadingDuration = 3.25;
const CGFloat kImagesSlidingInDuration = 1.0;
const CGFloat kSharingCancelledDuration = 0.5;

// Distance by which the profile images need to be moved when sliding.
const CGFloat kImagesSlidingOutDistance = 78;
const CGFloat kImagesSlidingInDistance = 51;

}  // namespace

@interface SharingStatusViewController ()

// Profile image of the sender.
@property(nonatomic, strong) UIImageView* senderImageView;
@property(nonatomic, strong) UIImage* senderImage;

// Profile image of the recipients.
@property(nonatomic, strong) UIImageView* recipientImage;

// Shield icon with a lock.
@property(nonatomic, strong) UIImageView* shieldLockImage;

// Rectangle view with fixed length and height containing fixed amount of
// circles.
@property(nonatomic, strong) UIView* progressBarView;

// Animates profile image of the sender sliding to the left and profile images
// of recipients sliding to the right.
@property(nonatomic, strong) UIViewPropertyAnimator* imagesSlidingOutAnimation;

// Animates shield lock appearing in the middle between profile images and the
// progress bar going from the left to right.
@property(nonatomic, strong)
    UIViewPropertyAnimator* progressBarLoadingAnimation;

// Animates progress bar and shield lock disappearing and profile images sliding
// to the middle.
@property(nonatomic, strong) UIViewPropertyAnimator* imagesSlidingInAnimation;

// Animates profile images sliding to the middle on cancel button tap.
@property(nonatomic, strong) UIViewPropertyAnimator* sharingCancelledAnimation;

// Contains the information that sharing is in progress at first and then is
// modified to convey the result status.
@property(nonatomic, strong) UILabel* titleLabel;

// Subtitle string that will be displayed when the sharing is succesful.
@property(nonatomic, strong) NSString* subtitleString;

// The button that cancels the sharing process.
@property(nonatomic, strong) UIButton* cancelButton;

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
  UIImageView* recipientImage = [self createRecipientImage];
  [animationView insertSubview:recipientImage belowSubview:senderImageView];

  // Add progress bar view.
  UIView* progressBarView = [self createProgressBarView];
  [animationView insertSubview:progressBarView belowSubview:recipientImage];

  // Add shield lock image.
  UIImageView* shieldLockImage = [self createShieldLockImage];
  [progressBarView addSubview:shieldLockImage];

  // Add progress bar circles.
  [self createProgressBarSubviews];

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
    [recipientImage.centerYAnchor
        constraintEqualToAnchor:senderImageView.centerYAnchor],
    [recipientImage.centerXAnchor
        constraintEqualToAnchor:senderImageView.centerXAnchor],

    // Progress bar constraints.
    [progressBarView.centerXAnchor
        constraintEqualToAnchor:senderImageView.centerXAnchor],
    [progressBarView.centerYAnchor
        constraintEqualToAnchor:senderImageView.centerYAnchor],
    [progressBarView.widthAnchor constraintEqualToConstant:kProgressBarWidth],
    [progressBarView.heightAnchor constraintEqualToConstant:kProgressBarHeight],

    // Shield lock image constraints.
    [shieldLockImage.centerYAnchor
        constraintEqualToAnchor:senderImageView.centerYAnchor],
    [shieldLockImage.centerXAnchor
        constraintEqualToAnchor:senderImageView.centerXAnchor],

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

#pragma mark - SharingStatusConsumer

- (void)setSenderImage:(UIImage*)senderImage {
  _senderImage = senderImage;
}

- (void)setSubtitleString:(NSString*)subtitleString {
  _subtitleString = subtitleString;
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
- (UIImageView*)createRecipientImage {
  // TODO(crbug.com/1463882): Add actual recipient icon.
  UIImageView* recipientImage = [[UIImageView alloc]
      initWithImage:DefaultSymbolTemplateWithPointSize(kPersonCropCircleSymbol,
                                                       kProfileImageSize)];
  recipientImage.translatesAutoresizingMaskIntoConstraints = NO;
  self.recipientImage = recipientImage;
  return recipientImage;
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

// Helper for creating the shield lock image view.
- (UIImageView*)createShieldLockImage {
  // TODO(crbug.com/1463882): Add correct shield image.
  UIImageView* shieldLockImage = [[UIImageView alloc]
      initWithImage:CustomSymbolWithPointSize(kShieldSymbol, kShieldLockSize)];
  shieldLockImage.translatesAutoresizingMaskIntoConstraints = NO;
  shieldLockImage.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  shieldLockImage.hidden = YES;
  self.shieldLockImage = shieldLockImage;
  return shieldLockImage;
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
  UIImageView* recipientImage = self.recipientImage;
  UIImageView* shieldLockImage = self.shieldLockImage;
  UIView* progressBarView = self.progressBarView;

  self.imagesSlidingOutAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kImagesSlidingOutDuration
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              senderImageView.hidden = NO;
              senderImageView.center = CGPointMake(
                  senderImageView.center.x - kImagesSlidingOutDistance,
                  senderImageView.center.y);
              recipientImage.center = CGPointMake(
                  recipientImage.center.x + kImagesSlidingOutDistance,
                  recipientImage.center.y);
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
              shieldLockImage.hidden = NO;

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
              shieldLockImage.hidden = YES;
              progressBarView.hidden = YES;
              senderImageView.center = CGPointMake(
                  senderImageView.center.x + kImagesSlidingInDistance,
                  senderImageView.center.y);
              recipientImage.center = CGPointMake(
                  recipientImage.center.x - kImagesSlidingInDistance,
                  recipientImage.center.y);
            }];
  __weak __typeof(self.delegate) weakDelegate = self.delegate;
  [self.imagesSlidingInAnimation
      addCompletion:^(UIViewAnimatingPosition finalPosition) {
        [weakSelf displaySuccessStatus];
        [weakDelegate startPasswordSharing];
      }];

  self.sharingCancelledAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kSharingCancelledDuration
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              shieldLockImage.hidden = YES;
              progressBarView.hidden = YES;
              senderImageView.center = CGPointMake(progressBarView.center.x,
                                                   senderImageView.center.y);
              recipientImage.center = CGPointMake(progressBarView.center.x,
                                                  recipientImage.center.y);
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
  view.editable = NO;
  view.selectable = YES;
  view.scrollEnabled = NO;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  return view;
}

// Adds link attribute to the specified `range` of the `view`.
- (void)addLinkAttributeToTextView:(UITextView*)view range:(NSRange)range {
  NSMutableAttributedString* newView = [[NSMutableAttributedString alloc]
      initWithAttributedString:view.attributedText];
  [newView addAttribute:NSLinkAttributeName value:@"" range:range];
  view.attributedText = newView;
}

// Helper to create the subtitle.
- (UITextView*)createSubtitle {
  UITextView* subtitle = [self createTextView];
  subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  subtitle.textColor = [UIColor colorNamed:kTextPrimaryColor];
  // TODO(crbug.com/1463882): Make parts of the string bold.
  StringWithTags stringWithTags = ParseStringWithLinks(self.subtitleString);
  subtitle.text = stringWithTags.string;
  [self addLinkAttributeToTextView:subtitle range:stringWithTags.ranges[0]];
  return subtitle;
}

// Helper to create the footer.
- (UITextView*)createFooter {
  UITextView* footer = [self createTextView];
  footer.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  footer.textColor = [UIColor colorNamed:kTextSecondaryColor];

  // TODO(crbug.com/1463882): Add passing link value.
  StringWithTags stringWithTags =
      ParseStringWithLinks(base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
          IDS_IOS_PASSWORD_SHARING_SUCCESS_FOOTNOTE, u"")));
  footer.text = stringWithTags.string;
  [self addLinkAttributeToTextView:footer range:stringWithTags.ranges[0]];

  return footer;
}

// Helper for creating the done button.
- (UIButton*)createDoneButton {
  UIButton* doneButton = PrimaryActionButton(YES);
  [doneButton addTarget:self
                 action:@selector(doneButtonTapped)
       forControlEvents:UIControlEventTouchUpInside];
  [doneButton setTitle:l10n_util::GetNSString(IDS_DONE)
              forState:UIControlStateNormal];
  doneButton.titleLabel.adjustsFontSizeToFitWidth = YES;
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

  [self.sharingCancelledAnimation startAnimation];
}

// Handles done buttons clicks by dismissing the view.
- (void)doneButtonTapped {
  [self.delegate sharingStatusWasDismissed:self];
}

@end
