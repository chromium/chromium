// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snackbar/ui_bundled/ui/snackbar_view.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/shared/ui/util/layout_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snackbar/ui_bundled/ui/snackbar_view_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Animation constants.
const NSTimeInterval kSnackbarAnimationDuration = 0.8;

// Snackbar constants.
const CGFloat kSnackbarCornerRadius = 16.0;
const CGFloat kHorizontalPadding = 16.0;
const CGFloat kVerticalPadding = 16.0;
const CGFloat kInterItemSpacing = 16.0;
const CGFloat kAccessoryViewSize = 32.0;
const CGFloat kSnackbarMarginLegacy = 8.0;
const CGFloat kSnackbarMarginNext = 4.0;
const CGFloat kSnackbarMinWidthRegular = 288.0;
const CGFloat kSnackbarMaxWidthRegular = 568.0;

// Button constants.
const CGFloat kButtonCornerRadius = 13.0;
const CGFloat kButtonMargin = 6.0;
const CGFloat kButtonBackgroundAlpha = 0.1;

// The vertical spacing between text labels.
const CGFloat kTextSpacing = 2.0;

}  // namespace

@interface SnackbarView () <UIGestureRecognizerDelegate>
@end

@implementation SnackbarView {
  // The content view that holds all the UI elements.
  UIView* _contentView;
  // The blur view that serves as the background.
  UIVisualEffectView* _blurView;
  // The stack view holding the title and subtitle.
  UIStackView* _textStackView;
  // The primary text label of the snackbar.
  UILabel* _titleLabel;
  // The optional secondary text label of the snackbar.
  UILabel* _subtitleLabel;
  // The optional image view displayed on the leading side.
  UIImageView* _leadingAccessoryView;
  // The optional image view displayed on the trailing side.
  UIImageView* _trailingAccessoryView;
  // The optional action button.
  UIButton* _button;
  // The bottom constraint of the snackbar.
  NSLayoutConstraint* _bottomConstraint;
  // The horizontal constraints for a compact layout.
  NSArray<NSLayoutConstraint*>* _compactWidthConstraints;
  // The horizontal constraints for a regular layout.
  NSArray<NSLayoutConstraint*>* _regularWidthConstraints;
  // Whether the message’s action was tapped.
  BOOL _actionTapped;
}

- (instancetype)initWithMessage:(SnackbarMessage*)message {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _message = message;
    _contentView = [[UIView alloc] init];
    _contentView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_contentView];
    [self setupView];
    [self setupGestureRecognizer];
    [self registerForTraitChanges];
  }
  return self;
}

#pragma mark - Public

- (void)presentAnimated:(BOOL)animated completion:(void (^)(void))completion {
  if (animated && !UIAccessibilityIsReduceMotionEnabled()) {
    self.alpha = 0;
    self.transform = CGAffineTransformMakeTranslation(
        0, self.bounds.size.height + self.bottomOffset);
    // Animate the snackbar sliding up from the bottom.
    [UIView animateWithDuration:kSnackbarAnimationDuration
        animations:^{
          self.alpha = 1;
          self.transform = CGAffineTransformIdentity;
        }
        completion:^(BOOL finished) {
          [self handlePresentationCompletedWithCompletion:completion];
        }];
  } else {
    [self handlePresentationCompletedWithCompletion:completion];
  }
}

- (void)dismissAnimated:(BOOL)animated completion:(void (^)(void))completion {
  if (animated && !UIAccessibilityIsReduceMotionEnabled()) {
    // Animate the snackbar sliding down off the screen.
    [UIView animateWithDuration:kSnackbarAnimationDuration
        animations:^{
          self.alpha = 0;
          self.transform = CGAffineTransformMakeTranslation(
              0, self.bounds.size.height + self.bottomOffset);
        }
        completion:^(BOOL finished) {
          if (completion) {
            completion();
          }
        }];
  } else {
    if (completion) {
      completion();
    }
  }
}

#pragma mark - UIView

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [self updateUserInterfaceStyle];
}

- (void)setBottomOffset:(CGFloat)bottomOffset {
  _bottomOffset = bottomOffset;
  const CGFloat margin =
      IsChromeNextIaEnabled() ? kSnackbarMarginNext : kSnackbarMarginLegacy;
  _bottomConstraint.constant = -(self.bottomOffset + margin);
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  if (self.superview) {
    [self setupSuperviewConstraints];
    [self updateConstraintsForSizeClass];
  }
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  // The snackbar view can be larger than its content view. Only intercept
  // touches that are within the content view.
  return CGRectContainsPoint(_contentView.frame, point);
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Don't handle taps on the action button.
  if (touch.view == _button) {
    return NO;
  }
  return YES;
}

#pragma mark - Private

// Main setup method.
- (void)setupView {
  _contentView.accessibilityIdentifier = kSnackbarAccessibilityId;
  _contentView.isAccessibilityElement = NO;
  if (IsChromeNextIaEnabled()) {
    _contentView.layer.cornerRadius = kAppBarCornerRadius - kSnackbarMarginNext;
  } else {
    _contentView.layer.cornerRadius = kSnackbarCornerRadius;
  }
  _contentView.clipsToBounds = YES;

  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemChromeMaterial];
  _blurView = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  _blurView.translatesAutoresizingMaskIntoConstraints = NO;
  [_contentView addSubview:_blurView];
  AddSameConstraints(_blurView, _contentView);

  // Setup optional views.
  [self setupLeadingAccessoryView];
  [self setupTrailingAccessoryView];
  [self setupButton];

  // Setup text labels.
  [self setupTextStackView];
  [self setupConstraints];
}

// Sets up the leading accessory view.
- (void)setupLeadingAccessoryView {
  if (!self.message.leadingAccessoryImage) {
    return;
  }
  _leadingAccessoryView =
      [[UIImageView alloc] initWithImage:self.message.leadingAccessoryImage];
  _leadingAccessoryView.translatesAutoresizingMaskIntoConstraints = NO;
  _leadingAccessoryView.accessibilityIdentifier =
      kSnackbarLeadingAccessoryAccessibilityId;
  _leadingAccessoryView.clipsToBounds = YES;
  if (self.message.roundLeadingAccessoryView) {
    _leadingAccessoryView.layer.cornerRadius = kAccessoryViewSize / 2;
  }
  _leadingAccessoryView.contentMode = UIViewContentModeCenter;
  [_blurView.contentView addSubview:_leadingAccessoryView];
}

// Sets up the trailing accessory view.
- (void)setupTrailingAccessoryView {
  if (!self.message.trailingAccessoryImage) {
    return;
  }
  _trailingAccessoryView =
      [[UIImageView alloc] initWithImage:self.message.trailingAccessoryImage];
  _trailingAccessoryView.translatesAutoresizingMaskIntoConstraints = NO;
  _trailingAccessoryView.accessibilityIdentifier =
      kSnackbarTrailingAccessoryAccessibilityId;
  _trailingAccessoryView.clipsToBounds = YES;
  if (self.message.roundTrailingAccessoryView) {
    _trailingAccessoryView.layer.cornerRadius = kAccessoryViewSize / 2;
  }
  _trailingAccessoryView.contentMode = UIViewContentModeCenter;
  [_blurView.contentView addSubview:_trailingAccessoryView];
}

// Sets up the text stack view.
- (void)setupTextStackView {
  _textStackView = [[UIStackView alloc] init];
  _textStackView.axis = UILayoutConstraintAxisVertical;
  _textStackView.spacing = kTextSpacing;
  _textStackView.distribution = UIStackViewDistributionEqualSpacing;
  _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [_blurView.contentView addSubview:_textStackView];

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.text = _message.title;
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _titleLabel.numberOfLines = 0;
  _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _titleLabel.isAccessibilityElement = YES;
  _titleLabel.accessibilityIdentifier = kSnackbarTitleAccessibilityId;
  [_textStackView addArrangedSubview:_titleLabel];

  if (self.message.subtitle) {
    _subtitleLabel = [self createSubtitleLabelWithText:self.message.subtitle];
    _subtitleLabel.isAccessibilityElement = YES;
    _subtitleLabel.accessibilityIdentifier = kSnackbarSubtitleAccessibilityId;
    [_textStackView addArrangedSubview:_subtitleLabel];
  }

  if (self.message.secondarySubtitle) {
    UILabel* secondarySubtitleLabel =
        [self createSubtitleLabelWithText:self.message.secondarySubtitle];
    secondarySubtitleLabel.isAccessibilityElement = YES;
    secondarySubtitleLabel.accessibilityIdentifier =
        kSnackbarSecondarySubtitleAccessibilityId;
    [_textStackView addArrangedSubview:secondarySubtitleLabel];
  }
}

// Sets up the action button.
- (void)setupButton {
  if (!_message.action) {
    return;
  }
  UIButtonConfiguration* config =
      [UIButtonConfiguration filledButtonConfiguration];
  UIFont* buttonFont = PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                                 UIFontWeightSemibold);
  NSDictionary* attributes = @{
    NSFontAttributeName : buttonFont,
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor]
  };
  NSAttributedString* attributedTitle =
      [[NSAttributedString alloc] initWithString:_message.action.title
                                      attributes:attributes];
  config.attributedTitle = attributedTitle;
  config.baseBackgroundColor =
      [[UIColor colorNamed:kInvertedPrimaryBackgroundColor]
          colorWithAlphaComponent:kButtonBackgroundAlpha];
  if (IsChromeNextIaEnabled()) {
    config.background.cornerRadius =
        _contentView.layer.cornerRadius - kButtonMargin;
  } else {
    config.background.cornerRadius = kButtonCornerRadius;
  }
  _button = [UIButton buttonWithConfiguration:config primaryAction:nil];
  _button.accessibilityLabel =
      _message.action.accessibilityLabel ?: _message.action.title;
  _button.accessibilityHint = _message.action.accessibilityHint;
  _button.accessibilityIdentifier = kSnackbarButtonAccessibilityId;
  _button.isAccessibilityElement = YES;
  [_button addTarget:self
                action:@selector(handleButtonTap)
      forControlEvents:UIControlEventTouchUpInside];
  _button.translatesAutoresizingMaskIntoConstraints = NO;
  [_blurView.contentView addSubview:_button];
}

// Sets up the layout constraints.
- (void)setupConstraints {
  // A snackbar can't have both an action button and a trailing accessory view.
  CHECK(!_button || !_trailingAccessoryView);

  // Leading constraints.
  if (_leadingAccessoryView) {
    [NSLayoutConstraint activateConstraints:@[
      [_leadingAccessoryView.leadingAnchor
          constraintEqualToAnchor:_contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_textStackView.leadingAnchor
          constraintEqualToAnchor:_leadingAccessoryView.trailingAnchor
                         constant:kInterItemSpacing],
    ]];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [_textStackView.leadingAnchor
          constraintEqualToAnchor:_contentView.leadingAnchor
                         constant:kHorizontalPadding],
    ]];
  }

  // Trailing constraints.
  UIView* trailingView = _button ?: _trailingAccessoryView;
  if (trailingView) {
    [trailingView setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisHorizontal];
    [trailingView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    CGFloat trailingConstant =
        (trailingView == _button) ? kButtonMargin : kHorizontalPadding;
    [NSLayoutConstraint activateConstraints:@[
      [trailingView.leadingAnchor
          constraintEqualToAnchor:_textStackView.trailingAnchor
                         constant:kInterItemSpacing],
      [_contentView.trailingAnchor
          constraintEqualToAnchor:trailingView.trailingAnchor
                         constant:trailingConstant],
      [trailingView.centerYAnchor
          constraintEqualToAnchor:_contentView.centerYAnchor],
    ]];
    if (trailingView == _button) {
      [NSLayoutConstraint activateConstraints:@[
        [trailingView.topAnchor constraintEqualToAnchor:_contentView.topAnchor
                                               constant:kButtonMargin],
        [trailingView.bottomAnchor
            constraintEqualToAnchor:_contentView.bottomAnchor
                           constant:-kButtonMargin],
      ]];
    } else {
      [NSLayoutConstraint activateConstraints:@[
        [trailingView.widthAnchor constraintEqualToConstant:kAccessoryViewSize],
        [trailingView.heightAnchor
            constraintEqualToConstant:kAccessoryViewSize],
      ]];
    }
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [_contentView.trailingAnchor
          constraintEqualToAnchor:_textStackView.trailingAnchor
                         constant:kHorizontalPadding],
    ]];
  }

  // Vertical constraints for the text stack view.
  [NSLayoutConstraint activateConstraints:@[
    [_textStackView.centerYAnchor
        constraintEqualToAnchor:_contentView.centerYAnchor],
    [_textStackView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_contentView.topAnchor
                                    constant:kVerticalPadding],
    [_textStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:_contentView.bottomAnchor
                                 constant:-kVerticalPadding],
  ]];

  // Leading accessory view constraints.
  if (_leadingAccessoryView) {
    [NSLayoutConstraint activateConstraints:@[
      [_leadingAccessoryView.centerYAnchor
          constraintEqualToAnchor:_contentView.centerYAnchor],
      [_leadingAccessoryView.widthAnchor
          constraintEqualToConstant:kAccessoryViewSize],
      [_leadingAccessoryView.heightAnchor
          constraintEqualToConstant:kAccessoryViewSize],
    ]];
  }

  // Ensure the height is at least twice the corner radius to maintain a capsule
  // look when chromeNext is enabled.
  if (IsChromeNextIaEnabled()) {
    [_contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:2 *
                                               _contentView.layer.cornerRadius]
        .active = YES;
  }
}

// Sets up the constraints with the superview.
- (void)setupSuperviewConstraints {
  UILayoutGuide* safeAreaLayoutGuide = self.safeAreaLayoutGuide;
  const CGFloat margin =
      IsChromeNextIaEnabled() ? kSnackbarMarginNext : kSnackbarMarginLegacy;
  _bottomConstraint = [_contentView.bottomAnchor
      constraintLessThanOrEqualToAnchor:self.bottomAnchor
                               constant:-(self.bottomOffset + margin)];
  _bottomConstraint.active = YES;

  [_contentView.bottomAnchor
      constraintLessThanOrEqualToAnchor:safeAreaLayoutGuide.bottomAnchor
                               constant:-margin]
      .active = YES;

  // On iPhone portrait, pin to the edges of the safe area.
  _compactWidthConstraints = @[
    [_contentView.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:margin],
    [_contentView.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor
                       constant:-margin],
  ];

  // On iPad or iPhone landscape, center the snackbar with a min and max width.
  _regularWidthConstraints = @[
    [_contentView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [_contentView.widthAnchor
        constraintLessThanOrEqualToConstant:kSnackbarMaxWidthRegular],
    [_contentView.widthAnchor
        constraintGreaterThanOrEqualToConstant:kSnackbarMinWidthRegular],
    [_contentView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                                    constant:margin],
    [_contentView.trailingAnchor
        constraintLessThanOrEqualToAnchor:safeAreaLayoutGuide.trailingAnchor
                                 constant:-margin],
  ];
}

// Updates the constraints based on the current size class.
- (void)updateConstraintsForSizeClass {
  if (IsCompactWidth(self)) {
    [NSLayoutConstraint deactivateConstraints:_regularWidthConstraints];
    [NSLayoutConstraint activateConstraints:_compactWidthConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_compactWidthConstraints];
    [NSLayoutConstraint activateConstraints:_regularWidthConstraints];
  }
}

// Registers observers for trait changes.
- (void)registerForTraitChanges {
  __weak __typeof(self) weakSelf = self;
  [self registerForTraitChanges:@[
    [UITraitUserInterfaceStyle class], [UITraitHorizontalSizeClass class]
  ]
                    withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                  UITraitCollection* previousTraitCollection) {
                      [weakSelf updateUserInterfaceStyle];
                      [weakSelf updateConstraintsForSizeClass];
                    }];
}

// Sets up the tap gesture recognizer.
- (void)setupGestureRecognizer {
  UITapGestureRecognizer* tapGestureRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleViewTap)];
  tapGestureRecognizer.delegate = self;
  [self addGestureRecognizer:tapGestureRecognizer];
}

// Updates the user interface style.
- (void)updateUserInterfaceStyle {
  // The snackbar's style is inverted. To get the app's actual style, we must
  // read from the window's trait collection, as this view's own
  // `traitCollection` is being overridden.
  if (!self.window) {
    return;
  }
  UIUserInterfaceStyle windowStyle =
      self.window.traitCollection.userInterfaceStyle;
  if (windowStyle == UIUserInterfaceStyleDark) {
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleLight;
  } else {
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }
}

// Creates and configures a subtitle label.
- (UILabel*)createSubtitleLabelWithText:(NSString*)text {
  UILabel* label = [[UILabel alloc] init];
  label.text = text;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.numberOfLines = 1;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.isAccessibilityElement = NO;
  return label;
}

// Handles the action button tap.
- (void)handleButtonTap {
  if (self.message.action.handler && !_actionTapped) {
    self.message.action.handler();
    _actionTapped = YES;
  }
  [self.delegate snackbarViewDidTapActionButton:self];
}

// Handles the view tap.
- (void)handleViewTap {
  [self.delegate snackbarViewDidRequestDismissal:self];
}

// Handles post-presentation tasks such as accessibility announcement and
// auto-dismissal.
- (void)handlePresentationCompletedWithCompletion:(void (^)(void))completion {
  if (UIAccessibilityIsVoiceOverRunning()) {
    [self postAccessibilityAnnouncement];
  }
  [self scheduleDismissal];
  if (completion) {
    completion();
  }
}

// Posts a high-priority accessibility announcement for the snackbar message.
- (void)postAccessibilityAnnouncement {
  if (!_titleLabel || !self.window) {
    return;
  }
  NSString* announcement = _message.title;
  if (_message.action) {
    NSString* actionTitle =
        _message.action.accessibilityLabel ?: _message.action.title;
    if (actionTitle.length) {
      announcement =
          [NSString stringWithFormat:@"%@. %@", _message.title, actionTitle];
    }
  }
  // Use high priority to prevent the announcement from being interrupted by
  // subsequent UI transitions or focus changes.
  NSAttributedString* queuedAnnouncement = [[NSAttributedString alloc]
      initWithString:announcement
          attributes:@{
            UIAccessibilitySpeechAttributeAnnouncementPriority :
                UIAccessibilityPriorityHigh
          }];
  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  queuedAnnouncement);
}

// Schedules the automatic dismissal of the snackbar.
- (void)scheduleDismissal {
  // Don't auto-dismiss if VoiceOver is running.
  if (UIAccessibilityIsVoiceOverRunning()) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               (int64_t)(self.message.duration * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [weakSelf.delegate snackbarViewDidRequestDismissal:weakSelf];
                 });
}

@end
