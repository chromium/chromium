// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/ui/synced_set_up_view_controller.h"

#import "base/functional/bind.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size (width and height) of the avatar image view.
const CGFloat kAvatarSize = 64.0;
// Margin between the bottom of the avatar and the top of the title label.
const CGFloat kTitleTopMargin = 16.0;
// Minimum margin between the bottom of the title and the top of the subtitle
// label.
const CGFloat kSubtitleMinTopMargin = 10.0;
// Preferred margin between the bottom of the title and the top of the subtitle
// label.
const CGFloat kSubtitlePreferredTopMargin = 122.0;
// Padding on the left and right sides of the main content view.
const CGFloat kHorizontalPadding = 20.0;
// The vertical translation offset for the entrance animations.
const CGFloat kEntranceAnimationTranslateY = 10.0;
// The duration of the entrance fade-in and slide animations.
constexpr base::TimeDelta kEntranceAnimationDuration = base::Seconds(0.5);
// The delay before the subtitle animation begins.
constexpr base::TimeDelta kSubtitleAnimationDelay = base::Seconds(0.15);
// The vertical translation offset for the exit animation.
const CGFloat kFadeOutAnimationTranslateY = 10.0;
// The delay before the fade-out animation begins.
constexpr base::TimeDelta kFadeOutAnimationDelay = base::Seconds(3.25);
// The duration of the fade-out animation.
constexpr base::TimeDelta kFadeOutAnimationDuration = base::Seconds(0.25);
// Accessibility identifier for the avatar image view.
NSString* const kSyncedSetUpAvatarAccessibilityID =
    @"kSyncedSetUpAvatarAccessibilityID";
// Accessibility identifier for the title label.
NSString* const kSyncedSetUpTitleAccessibilityID =
    @"kSyncedSetUpTitleAccessibilityID";
// Accessibility identifier for the subtitle label.
NSString* const kSyncedSetUpSubtitleAccessibilityID =
    @"kSyncedSetUpSubtitleAccessibilityID";
// Lottie animation file name.
NSString* const kSpinAnimationName = @"synced_set_up_spin";

// Helper function to configure common label properties.
static void ConfigureCommonLabelProperties(UILabel* label) {
  label.textAlignment = NSTextAlignmentCenter;
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
}

}  // namespace

@implementation SyncedSetUpViewController {
  // Scroll view to contain all content.
  UIScrollView* _scrollView;
  // A container view inside the scroll view to hold the stack view.
  UIView* _contentView;
  // Container stack view to group and center the elements.
  UIStackView* _stackView;
  // Displays the user's avatar image.
  UIImageView* _avatarImageView;
  // Displays the main welcome message/title.
  UILabel* _titleLabel;
  // Displays additional text below the title.
  UILabel* _subtitleLabel;
  // The welcome message to display. Stored in case it is set before the view
  // loads.
  NSString* _welcomeMessage;
  // The avatar image to display. Stored in case it is set before the view
  // loads.
  UIImage* _avatarImage;
  // Tracks if the animations have been started.
  BOOL _hasStartedAnimations;
  // The Lottie animation that plays to reveal the avatar.
  id<LottieAnimation> _spinAnimation;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  _spinAnimation = [self createAnimation:kSpinAnimationName];

  [self setupViews];
  [self setupConstraints];

  // Sets the initial state for animations.
  _titleLabel.alpha = 0.0;
  _subtitleLabel.alpha = 0.0;
  _hasStartedAnimations = NO;

  // Updates the UI elements with the current state (which may have been set
  // before `-viewDidLoad`).
  [self updateTitleLabel];
  [self updateAvatarImageView];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  if (!_hasStartedAnimations) {
    __weak __typeof(self) weakSelf = self;
    [self animateTitleIn];
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf animateSubtitleIn];
        }),
        kSubtitleAnimationDelay);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf animateAllElementsOut];
        }),
        kFadeOutAnimationDelay);
    [_spinAnimation play];
    _hasStartedAnimations = YES;
  }
}

#pragma mark - SyncedSetUpConsumer

- (void)setWelcomeMessage:(NSString*)message {
  if ([_welcomeMessage isEqualToString:message]) {
    return;
  }

  _welcomeMessage = [message copy];

  if (self.isViewLoaded) {
    [self updateTitleLabel];
  }
}

- (void)setAvatarImage:(UIImage*)image {
  if ([_avatarImage isEqual:image]) {
    return;
  }

  _avatarImage = image;

  if (self.isViewLoaded) {
    [self updateAvatarImageView];
  }
}

#pragma mark - Private

// Creates a Lottie animation view.
- (id<LottieAnimation>)createAnimation:(NSString*)animationName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationName;
  config.shouldLoop = NO;
  id<LottieAnimation> animation =
      ios::provider::GenerateLottieAnimation(config);
  animation.animationView.translatesAutoresizingMaskIntoConstraints = NO;
  return animation;
}

// Updates the title label based on `_welcomeMessage`.
- (void)updateTitleLabel {
  if (_welcomeMessage) {
    _titleLabel.text = _welcomeMessage;
    return;
  }

  // Fallback to the default title if no message has been set yet.
  _titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_SYNCED_SET_UP_WELCOME_MESSAGE_TITLE);
}

// Updates the avatar image view based on `_avatarImage`.
- (void)updateAvatarImageView {
  if (_avatarImage) {
    UIImage* circularAvatar = CircularImageFromImage(_avatarImage, kAvatarSize);
    _avatarImageView.image = circularAvatar;
    return;
  }

  _avatarImageView.image = nil;
}

// Creates and returns a configured avatar image view.
- (UIImageView*)createAvatarImageView {
  UIImageView* avatarImageView = [[UIImageView alloc] init];
  avatarImageView.contentMode = UIViewContentModeScaleAspectFill;
  avatarImageView.translatesAutoresizingMaskIntoConstraints = NO;
  avatarImageView.clipsToBounds = YES;
  avatarImageView.isAccessibilityElement = YES;
  avatarImageView.accessibilityIdentifier = kSyncedSetUpAvatarAccessibilityID;
  AddSquareConstraints(avatarImageView, kAvatarSize);
  return avatarImageView;
}

// Creates and returns a configured title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  ConfigureCommonLabelProperties(titleLabel);
  titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_SYNCED_SET_UP_WELCOME_MESSAGE_TITLE);
  titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleTitle1, UIFontWeightBold);
  titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  titleLabel.accessibilityIdentifier = kSyncedSetUpTitleAccessibilityID;
  return titleLabel;
}

// Creates and returns a configured subtitle label.
- (UILabel*)createSubtitleLabel {
  UILabel* subtitleLabel = [[UILabel alloc] init];
  ConfigureCommonLabelProperties(subtitleLabel);
  subtitleLabel.text =
      l10n_util::GetNSString(IDS_IOS_SYNCED_SET_UP_SUBTITLE_TEXT);
  subtitleLabel.font = PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                                 UIFontWeightRegular);
  subtitleLabel.accessibilityIdentifier = kSyncedSetUpSubtitleAccessibilityID;
  return subtitleLabel;
}

// Creates and returns a spacer view for the stack view.
- (UIView*)createSpacerView {
  UIView* spacerView = [[UIView alloc] init];
  spacerView.translatesAutoresizingMaskIntoConstraints = NO;
  NSLayoutConstraint* spacerMinHeight = [spacerView.heightAnchor
      constraintGreaterThanOrEqualToConstant:kSubtitleMinTopMargin];
  NSLayoutConstraint* spacerPreferredHeight = [spacerView.heightAnchor
      constraintEqualToConstant:kSubtitlePreferredTopMargin];
  spacerPreferredHeight.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint
      activateConstraints:@[ spacerMinHeight, spacerPreferredHeight ]];
  return spacerView;
}

// Creates and configures the subviews within a scroll view.
- (void)setupViews {
  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_scrollView];

  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_contentView];

  _avatarImageView = [self createAvatarImageView];
  _titleLabel = [self createTitleLabel];
  _subtitleLabel = [self createSubtitleLabel];
  UIView* spacerView = [self createSpacerView];

  _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _avatarImageView, _titleLabel, spacerView, _subtitleLabel
  ]];

  _stackView.axis = UILayoutConstraintAxisVertical;
  _stackView.alignment = UIStackViewAlignmentCenter;
  _stackView.distribution = UIStackViewDistributionFill;
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;

  [_stackView setCustomSpacing:kTitleTopMargin afterView:_avatarImageView];

  [_contentView addSubview:_stackView];

  // Add animation view on top of all other views.
  [self.view addSubview:_spinAnimation.animationView];
}

// Configures constraints to support the scroll view structure.
- (void)setupConstraints {
  UILayoutGuide* safeArea = self.view.safeAreaLayoutGuide;
  AddSameConstraints(_scrollView, safeArea);

  UILayoutGuide* contentLayoutGuide = _scrollView.contentLayoutGuide;
  AddSameConstraints(_contentView, contentLayoutGuide);

  UILayoutGuide* frameLayoutGuide = _scrollView.frameLayoutGuide;

  [_contentView.widthAnchor
      constraintEqualToAnchor:frameLayoutGuide.widthAnchor]
      .active = YES;

  NSLayoutConstraint* contentHeightConstraint = [_contentView.heightAnchor
      constraintGreaterThanOrEqualToAnchor:frameLayoutGuide.heightAnchor];
  contentHeightConstraint.active = YES;

  AddSameCenterConstraints(_stackView, _contentView);
  AddSameConstraints(_avatarImageView, _spinAnimation.animationView);

  [NSLayoutConstraint activateConstraints:@[
    [_stackView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_contentView.topAnchor],
    [_stackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:_contentView.bottomAnchor],
    [_stackView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_contentView.leadingAnchor
                                    constant:kHorizontalPadding],
    [_stackView.trailingAnchor
        constraintLessThanOrEqualToAnchor:_contentView.trailingAnchor
                                 constant:-kHorizontalPadding],
  ]];
}

// Animates the title label with a fade-in and upward slide effect.
- (void)animateTitleIn {
  // A slight downward translation is applied to create the upward animation.
  _titleLabel.transform =
      CGAffineTransformMakeTranslation(0, kEntranceAnimationTranslateY);

  __weak __typeof(_titleLabel) weakTitleLabel = _titleLabel;
  [UIView animateWithDuration:kEntranceAnimationDuration.InSecondsF()
                   animations:^{
                     weakTitleLabel.alpha = 1.0;
                     weakTitleLabel.transform = CGAffineTransformIdentity;
                   }];
}

// Animates the subtitle label with a fade-in and upward slide effect.
- (void)animateSubtitleIn {
  // A slight downward translation is applied to create the upward animation.
  _subtitleLabel.transform =
      CGAffineTransformMakeTranslation(0, kEntranceAnimationTranslateY);

  __weak __typeof(_subtitleLabel) weakSubtitleLabel = _subtitleLabel;
  [UIView animateWithDuration:kEntranceAnimationDuration.InSecondsF()
                   animations:^{
                     weakSubtitleLabel.alpha = 1.0;
                     weakSubtitleLabel.transform = CGAffineTransformIdentity;
                   }];
}

// Animates the avatar, title, and subtitle out with a fade and downward slide.
- (void)animateAllElementsOut {
  __weak __typeof(_spinAnimation.animationView) weakAnimationView =
      _spinAnimation.animationView;
  __weak __typeof(_avatarImageView) weakAvatarImageView = _avatarImageView;
  __weak __typeof(_titleLabel) weakTitleLabel = _titleLabel;
  __weak __typeof(_subtitleLabel) weakSubtitleLabel = _subtitleLabel;

  [UIView animateWithDuration:kFadeOutAnimationDuration.InSecondsF()
                   animations:^{
                     CGAffineTransform downwardTransform =
                         CGAffineTransformMakeTranslation(
                             0, kFadeOutAnimationTranslateY);
                     weakAnimationView.alpha = 0.0;
                     weakAnimationView.transform = downwardTransform;
                     weakAvatarImageView.alpha = 0.0;
                     weakAvatarImageView.transform = downwardTransform;
                     weakTitleLabel.alpha = 0.0;
                     weakTitleLabel.transform = downwardTransform;
                     weakSubtitleLabel.alpha = 0.0;
                     weakSubtitleLabel.transform = downwardTransform;
                   }];
}

@end
