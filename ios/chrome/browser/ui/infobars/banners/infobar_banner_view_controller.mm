// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"

#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Banner View constants.
const CGFloat kBannerViewCornerRadius = 13.0;
const CGFloat kBannerViewYShadowOffset = 3.0;
const CGFloat kBannerViewShadowRadius = 9.0;
const CGFloat kBannerViewShadowOpacity = 0.23;

// Banner View selected constants.
const CGFloat kTappedBannerViewScale = 0.98;
const CGFloat kSelectedBannerViewScale = 1.02;
constexpr base::TimeDelta kSelectBannerAnimationDuration =
    base::Milliseconds(200);
constexpr base::TimeDelta kTappedBannerAnimationDuration =
    base::Milliseconds(50);
const CGFloat kSelectedBannerViewYShadowOffset = 8.0;

// Button constants.
const CGFloat kButtonWidth = 100.0;
const CGFloat kButtonSeparatorWidth = 1.0;
const CGFloat kButtonMaxFontSize = 45;

// Container Stack constants.
const CGFloat kContainerStackSpacing = 10.0;
const CGFloat kContainerStackVerticalPadding = 18.0;
const CGFloat kContainerStackHorizontalPadding = 15.0;

// Labels stack constants.
const CGFloat kLabelsStackViewVerticalSpacing = 2.0;

// Icon constants.
const CGFloat kIconCornerRadius = 5.0;
const CGFloat kCustomSpacingAfterIcon = 14.0;

// Favicon constants.
const CGFloat kFaviconShadowRadius = 3.0;
const CGFloat kFaviconShadowOpacity = 0.2;
const CGFloat kFaviconShadowYOffset = 1;
const CGFloat kFaviconSize = 24.0;
const CGFloat kFavIconCornerRadius = 5.0;
const CGFloat kFaviconContainerSize = 36.0;
const CGFloat kFavIconContainerCornerRadius = 7.0;

// Gesture constants.
const CGFloat kChangeInPositionForDismissal = -15.0;
constexpr base::TimeDelta kLongPressTimeDuration = base::Milliseconds(400);
}  // namespace

@interface InfobarBannerViewController ()

// Properties backing the InfobarBannerConsumer protocol.
@property(nonatomic, copy) NSString* bannerAccessibilityLabel;
@property(nonatomic, copy) NSString* buttonText;
@property(nonatomic, strong) UIImage* faviconImage;
@property(nonatomic, strong) UIImage* iconImage;
@property(nonatomic, assign) BOOL presentsModal;
@property(nonatomic, copy) NSString* titleText;
@property(nonatomic, copy) NSString* subtitleText;
@property(nonatomic, assign) BOOL useIconBackgroundTint;
@property(nonatomic, assign) BOOL ignoreIconColorWithTint;
@property(nonatomic, strong) UIColor* iconImageTintColor;
@property(nonatomic, strong) UIColor* iconBackgroundColor;
@property(nonatomic, assign) NSInteger titleNumberOfLines;
@property(nonatomic, assign) NSInteger subtitleNumberOfLines;
@property(nonatomic, assign) NSLineBreakMode subtitleLineBreakMode;

// The original position of this InfobarVC view in the parent's view coordinate
// system.
@property(nonatomic, assign) CGPoint originalCenter;
// The starting point of the LongPressGesture, used to calculate the gesture
// translation.
@property(nonatomic, assign) CGPoint startingTouch;
// Delegate to handle this InfobarVC actions.
@property(nonatomic, weak) id<InfobarBannerDelegate> delegate;
// YES if the user is interacting with the view via a touch gesture.
@property(nonatomic, assign) BOOL touchInProgress;
// YES if the view should be dismissed after any touch gesture has ended.
@property(nonatomic, assign) BOOL shouldDismissAfterTouchesEnded;
// UIButton which opens the modal.
@property(nonatomic, strong) UIButton* openModalButton;
// UIButton with title `self.buttonText`, which triggers the Infobar action.
@property(nonatomic, strong) UIButton* infobarButton;
// UILabel displaying `self.titleText`.
@property(nonatomic, strong) UILabel* titleLabel;
// UILabel displaying `self.subTitleText`.
@property(nonatomic, strong) UILabel* subTitleLabel;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;
// The time in which the Banner appeared on screen.
@property(nonatomic, assign) base::TimeTicks bannerAppearedTime;
// YES if the banner on screen time metric has already been recorded for this
// banner.
@property(nonatomic, assign) BOOL bannerOnScreenTimeWasRecorded;

@end

@implementation InfobarBannerViewController
// Synthesized from InfobarBannerInteractable.
@synthesize interactionDelegate = _interactionDelegate;

- (instancetype)initWithDelegate:(id<InfobarBannerDelegate>)delegate
                   presentsModal:(BOOL)presentsModal
                            type:(InfobarType)infobarType {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _delegate = delegate;
    _metricsRecorder =
        [[InfobarMetricsRecorder alloc] initWithType:infobarType];
    _presentsModal = presentsModal;
    _useIconBackgroundTint = YES;
    _ignoreIconColorWithTint = YES;
    _subtitleLineBreakMode = NSLineBreakByTruncatingTail;
  }
  return self;
}

#pragma mark - View Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  // BannerView setup.
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.view.layer.cornerRadius = kBannerViewCornerRadius;
  [self.view.layer setShadowOffset:CGSizeMake(0.0, kBannerViewYShadowOffset)];
  [self.view.layer setShadowRadius:kBannerViewShadowRadius];
  [self.view.layer setShadowOpacity:kBannerViewShadowOpacity];
  // If dark mode is set when the banner is presented, the semantic color will
  // need to be set here.
  [self.traitCollection performAsCurrentTraitCollection:^{
    [self.view.layer
        setShadowColor:[UIColor colorNamed:kToolbarShadowColor].CGColor];
  }];
  self.view.accessibilityIdentifier = kInfobarBannerViewIdentifier;
  self.view.accessibilityCustomActions = [self accessibilityActions];

  // Icon setup.
  UIView* iconContainerView = nil;
  if (self.faviconImage) {
    iconContainerView = [self configureFaviconImageContainer];
  }
  if (self.iconImage) {
    iconContainerView = [self configureIconImageContainer];
  }

  // Labels setup.
  self.titleLabel = [[UILabel alloc] init];
  self.titleLabel.text = self.titleText;
  self.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  self.titleLabel.numberOfLines = _titleNumberOfLines;
  self.titleLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;
  [self.titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  self.subTitleLabel = [[UILabel alloc] init];
  self.subTitleLabel.text = self.subtitleText;
  self.subTitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  self.subTitleLabel.adjustsFontForContentSizeCategory = YES;
  self.subTitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  self.subTitleLabel.numberOfLines = _subtitleNumberOfLines;
  self.subTitleLabel.lineBreakMode = _subtitleLineBreakMode;

  // If `self.subTitleText` hasn't been set or is empty, hide the label to keep
  // the title label centered in the Y axis.
  self.subTitleLabel.hidden = !self.subtitleText.length;

  UIStackView* labelsStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ self.titleLabel, self.subTitleLabel ]];
  labelsStackView.axis = UILayoutConstraintAxisVertical;
  labelsStackView.layoutMarginsRelativeArrangement = YES;
  labelsStackView.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      kContainerStackVerticalPadding, 0, kContainerStackVerticalPadding, 0);
  labelsStackView.spacing = kLabelsStackViewVerticalSpacing;
  labelsStackView.accessibilityIdentifier =
      kInfobarBannerLabelsStackViewIdentifier;
  labelsStackView.isAccessibilityElement = YES;
  labelsStackView.accessibilityLabel = [self accessibilityLabel];

  // Button setup.
  self.infobarButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [self.infobarButton setTitle:self.buttonText forState:UIControlStateNormal];
  self.infobarButton.titleLabel.font = [[UIFontMetrics defaultMetrics]
      scaledFontForFont:[UIFont
                            preferredFontForTextStyle:UIFontTextStyleHeadline]
       maximumPointSize:kButtonMaxFontSize];
  self.infobarButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.infobarButton.titleLabel.numberOfLines = 0;
  self.infobarButton.titleLabel.textAlignment = NSTextAlignmentCenter;
  [self.infobarButton addTarget:self
                         action:@selector(bannerInfobarButtonWasPressed:)
               forControlEvents:UIControlEventTouchUpInside];
  self.infobarButton.accessibilityIdentifier =
      kInfobarBannerAcceptButtonIdentifier;
  self.infobarButton.pointerInteractionEnabled = YES;
  self.infobarButton.layer.cornerRadius = kBannerViewCornerRadius;
  self.infobarButton.clipsToBounds = YES;
  self.infobarButton.pointerStyleProvider =
      ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                       UIPointerShape* proposedShape) {
    UIPointerShape* shape =
        [UIPointerShape shapeWithRoundedRect:button.frame
                                cornerRadius:kBannerViewCornerRadius];
    return [UIPointerStyle styleWithEffect:proposedEffect shape:shape];
  };

  UIView* buttonSeparator = [[UIView alloc] init];
  buttonSeparator.translatesAutoresizingMaskIntoConstraints = NO;
  buttonSeparator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  [self.infobarButton addSubview:buttonSeparator];

  // Container Stack setup.
  UIStackView* containerStack = [[UIStackView alloc] init];
  // Check if it should have an icon.
  if (iconContainerView) {
    [containerStack addArrangedSubview:iconContainerView];
    [containerStack setCustomSpacing:kCustomSpacingAfterIcon
                           afterView:iconContainerView];
  }
  // Add labels.
  [containerStack addArrangedSubview:labelsStackView];
    // Open Modal Button setup.
  self.openModalButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  UIImage* gearImage = DefaultSymbolWithPointSize(kSettingsFilledSymbol,
                                                  kInfobarSymbolPointSize);

  [self.openModalButton setImage:gearImage forState:UIControlStateNormal];
  self.openModalButton.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  [self.openModalButton addTarget:self
                           action:@selector(animateBannerTappedAndPresentModal)
                 forControlEvents:UIControlEventTouchUpInside];
  [self.openModalButton
      setContentHuggingPriority:UILayoutPriorityDefaultHigh
                        forAxis:UILayoutConstraintAxisHorizontal];
  [self.openModalButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  self.openModalButton.accessibilityIdentifier =
      kInfobarBannerOpenModalButtonIdentifier;
  self.openModalButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BANNER_OPTIONS_HINT);
  [containerStack addArrangedSubview:self.openModalButton];
  // Hide open modal button if user shouldn't be allowed to open the modal.
  self.openModalButton.hidden = !self.presentsModal;
  self.openModalButton.pointerInteractionEnabled = YES;
  self.openModalButton.layer.cornerRadius = gearImage.size.width / 2;
  self.openModalButton.pointerStyleProvider =
      CreateDefaultEffectCirclePointerStyleProvider();

  // Add accept button.
  [containerStack addArrangedSubview:self.infobarButton];
  // Configure it.
  containerStack.axis = UILayoutConstraintAxisHorizontal;
  containerStack.spacing = kContainerStackSpacing;
  containerStack.distribution = UIStackViewDistributionFill;
  containerStack.alignment = UIStackViewAlignmentCenter;
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.insetsLayoutMarginsFromSafeArea = NO;
  [self.view addSubview:containerStack];

  // Constraints setup.
  [NSLayoutConstraint activateConstraints:@[
    // Container Stack.
    [containerStack.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kContainerStackHorizontalPadding],
    [containerStack.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [containerStack.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [containerStack.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    // Button.
    [self.infobarButton.widthAnchor constraintEqualToConstant:kButtonWidth],
    [self.infobarButton.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.infobarButton.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [buttonSeparator.widthAnchor
        constraintEqualToConstant:kButtonSeparatorWidth],
    [buttonSeparator.leadingAnchor
        constraintEqualToAnchor:self.infobarButton.leadingAnchor],
    [buttonSeparator.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [buttonSeparator.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    // Open modal button.
    [NSLayoutConstraint constraintWithItem:self.openModalButton
                                 attribute:NSLayoutAttributeHeight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self.openModalButton
                                 attribute:NSLayoutAttributeWidth
                                multiplier:1
                                  constant:0],
  ]];

  // Gestures setup.
  UIPanGestureRecognizer* panGestureRecognizer =
      [[UIPanGestureRecognizer alloc] init];
  [panGestureRecognizer addTarget:self action:@selector(handleGestures:)];
  [panGestureRecognizer setMaximumNumberOfTouches:1];
  [self.view addGestureRecognizer:panGestureRecognizer];

  UILongPressGestureRecognizer* longPressGestureRecognizer =
      [[UILongPressGestureRecognizer alloc] init];
  [longPressGestureRecognizer addTarget:self action:@selector(handleGestures:)];
  longPressGestureRecognizer.minimumPressDuration =
      kLongPressTimeDuration.InSecondsF();
  [self.view addGestureRecognizer:longPressGestureRecognizer];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
      UITraitUserInterfaceIdiom.self, UITraitUserInterfaceStyle.self,
      UITraitDisplayGamut.self, UITraitAccessibilityContrast.self,
      UITraitUserInterfaceLevel.self
    ]);
    __weak __typeof(self) weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      [weakSelf updateShadowColorOnTraitChange:previousCollection];
    };
    [self registerForTraitChanges:traits withHandler:handler];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Presented];
  self.bannerAppearedTime = base::TimeTicks::Now();
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  // Call recordBannerOnScreenTime on viewWillDisappear since viewDidDisappear
  // is called after the dismissal animation has occured.
  [self recordBannerOnScreenTime];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Dismissed];
  // If the delegate exists at the time of dismissal it should handle the
  // dismissal cleanup. Otherwise the BannerContainer needs to be informed that
  // this banner was dismissed in case it needs to present a queued one.
  if (self.delegate) {
    [self.delegate infobarBannerWasDismissed];
  }
  [super viewDidDisappear:animated];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
// This is triggered when dark mode changes while the banner is already
// presented.
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  [self updateShadowColorOnTraitChange:previousTraitCollection];
}
#endif

#pragma mark - Public Methods

- (void)dismissWhenInteractionIsFinished {
  if (!self.touchInProgress) {
    [self.metricsRecorder
        recordBannerDismissType:MobileMessagesBannerDismissType::TimedOut];
    [self.delegate dismissInfobarBannerForUserInteraction:NO];
  }
  self.shouldDismissAfterTouchesEnded = YES;
}

#pragma mark - Setters

- (void)setTitleText:(NSString*)titleText {
  _titleText = titleText;
  self.titleLabel.text = _titleText;
}

- (void)setSubtitleText:(NSString*)subtitleText {
  _subtitleText = subtitleText;
  self.subTitleLabel.text = _subtitleText;
  self.subTitleLabel.hidden = !self.subtitleText.length;
}

- (void)setButtonText:(NSString*)buttonText {
  _buttonText = buttonText;
  [self.infobarButton setTitle:_buttonText forState:UIControlStateNormal];
}

- (void)setPresentsModal:(BOOL)presentsModal {
  // TODO(crbug.com/40626691): Write a test for setting this to NO;
  if (_presentsModal == presentsModal)
    return;
  _presentsModal = presentsModal;
  self.openModalButton.hidden = !presentsModal;
  self.view.accessibilityCustomActions = [self accessibilityActions];
}

- (void)setIconImageTintColor:(UIColor*)iconImageTintColor {
  _iconImageTintColor = iconImageTintColor;
}

- (void)setUseIconBackgroundTint:(BOOL)useIconBackgroundTint {
  _useIconBackgroundTint = useIconBackgroundTint;
}

- (void)setIgnoreIconColorWithTint:(BOOL)ignoreIconColorWithTint {
  _ignoreIconColorWithTint = ignoreIconColorWithTint;
}

- (void)setIconBackgroundColor:(UIColor*)iconBackgroundColor {
  _iconBackgroundColor = iconBackgroundColor;
}

#pragma mark - Private Methods

// Configures and returns the UIView that contains the `faviconImage`.
- (UIView*)configureFaviconImageContainer {
  DCHECK(!self.iconImage);

  UIView* faviconContainerView = [[UIView alloc] init];
  faviconContainerView.layer.shadowColor = [UIColor blackColor].CGColor;
  faviconContainerView.layer.shadowOffset =
      CGSizeMake(0, kFaviconShadowYOffset);
  faviconContainerView.layer.shadowRadius = kFaviconShadowRadius;
  faviconContainerView.layer.shadowOpacity = kFaviconShadowOpacity;

  UIView* faviconBackgroundContainerView = [[UIView alloc] init];
  faviconBackgroundContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconBackgroundContainerView.layer.cornerRadius =
      kFavIconContainerCornerRadius;
  faviconBackgroundContainerView.backgroundColor =
      [UIColor colorNamed:kBackgroundColor];
  [faviconContainerView addSubview:faviconBackgroundContainerView];

  UIImageView* faviconImageView =
      [[UIImageView alloc] initWithImage:self.faviconImage];
  faviconImageView.clipsToBounds = YES;
  faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconImageView.layer.cornerRadius = kFavIconCornerRadius;
  faviconImageView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  [faviconBackgroundContainerView addSubview:faviconImageView];

  [NSLayoutConstraint activateConstraints:@[
    [faviconContainerView.widthAnchor
        constraintEqualToConstant:kFaviconContainerSize],
    [faviconContainerView.heightAnchor
        constraintEqualToConstant:kFaviconContainerSize],
    [faviconImageView.widthAnchor constraintEqualToConstant:kFaviconSize],
    [faviconImageView.heightAnchor constraintEqualToConstant:kFaviconSize],
  ]];
  AddSameConstraints(faviconContainerView, faviconBackgroundContainerView);
  AddSameCenterConstraints(faviconContainerView, faviconImageView);

  return faviconContainerView;
}

// Configures and returns the UIView that contains the `iconImage`.
- (UIView*)configureIconImageContainer {
  DCHECK(!self.faviconImage);

  // If the icon image requires a background tint, ignore the original color
  // information and draw the image as a template image.
  if (self.useIconBackgroundTint && self.ignoreIconColorWithTint) {
    self.iconImage = [self.iconImage
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }
  UIImageView* iconImageView =
      [[UIImageView alloc] initWithImage:self.iconImage];
  iconImageView.contentMode = UIViewContentModeScaleAspectFit;
  iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  iconImageView.tintColor = self.iconImageTintColor;

  UIView* backgroundIconView =
      [[UIView alloc] initWithFrame:iconImageView.frame];
  backgroundIconView.layer.cornerRadius = kIconCornerRadius;
  if (self.useIconBackgroundTint) {
    backgroundIconView.backgroundColor =
        self.iconBackgroundColor ? self.iconBackgroundColor
                                 : [UIColor colorNamed:kBlueHaloColor];
  }
  backgroundIconView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* iconContainerView = [[UIView alloc] init];
  [iconContainerView addSubview:backgroundIconView];
  [iconContainerView addSubview:iconImageView];
  iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [backgroundIconView.widthAnchor
        constraintEqualToConstant:kInfobarBannerIconSize],
    [backgroundIconView.heightAnchor
        constraintEqualToConstant:kInfobarBannerIconSize],

    [iconImageView.widthAnchor
        constraintEqualToConstant:kInfobarBannerIconSize],
    [iconContainerView.widthAnchor
        constraintEqualToAnchor:backgroundIconView.widthAnchor],
  ]];
  AddSameCenterConstraints(iconContainerView, backgroundIconView);
  AddSameCenterConstraints(iconContainerView, iconImageView);

  return iconContainerView;
}

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  [self.interactionDelegate infobarBannerStartedInteraction];
  [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Accepted];
  [self.delegate bannerInfobarButtonWasPressed:sender];
}

- (void)handleGestures:(UILongPressGestureRecognizer*)gesture {
  CGPoint touchLocation = [gesture locationInView:self.view];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    [self.interactionDelegate infobarBannerStartedInteraction];
    [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Handled];
    self.originalCenter = self.view.center;
    self.touchInProgress = YES;
    self.startingTouch = touchLocation;
    [self animateBannerToScaleUpState];
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    // Don't allow the banner to be dragged down past its original position.
    CGFloat newYPosition =
        self.view.center.y + touchLocation.y - self.startingTouch.y;
    if (newYPosition < self.originalCenter.y) {
      self.view.center = CGPointMake(self.view.center.x, newYPosition);
    }
  }

  if (gesture.state == UIGestureRecognizerStateEnded) {
    [self
        animateBannerToOriginalStateWithDuration:kSelectBannerAnimationDuration
                                      completion:nil];
    // If dragged up by more than kChangeInPositionForDismissal at the time
    // the gesture ended, OR `self.shouldDismissAfterTouchesEnded` is YES.
    // Dismiss the banner.
    BOOL dragUpExceededThreshold = (self.view.center.y - self.originalCenter.y -
                                        kChangeInPositionForDismissal <
                                    0);
    if (dragUpExceededThreshold || self.shouldDismissAfterTouchesEnded) {
      if (dragUpExceededThreshold) {
        [self.metricsRecorder
            recordBannerDismissType:MobileMessagesBannerDismissType::SwipedUp];
        [self.delegate dismissInfobarBannerForUserInteraction:YES];
      } else {
        [self.metricsRecorder
            recordBannerDismissType:MobileMessagesBannerDismissType::TimedOut];
        [self.delegate dismissInfobarBannerForUserInteraction:NO];
      }
    } else {
      [self.metricsRecorder
          recordBannerEvent:MobileMessagesBannerEvent::ReturnedToOrigin];
      [self animateBannerToOriginalPosition];
    }
  }

  if (gesture.state == UIGestureRecognizerStateCancelled) {
    // Reset the superview transform so its frame is valid again.
    self.view.superview.transform = CGAffineTransformIdentity;
  }
}

// Animate the Banner being selected by scaling it up.
- (void)animateBannerToScaleUpState {
  [UIView animateWithDuration:kSelectBannerAnimationDuration.InSecondsF()
                   animations:^{
                     self.view.superview.transform = CGAffineTransformMakeScale(
                         kSelectedBannerViewScale, kSelectedBannerViewScale);
                     [self.view.layer
                         setShadowOffset:CGSizeMake(
                                             0.0,
                                             kSelectedBannerViewYShadowOffset)];
                   }
                   completion:nil];
}

// Animate the Banner back to its original size and styling.
- (void)animateBannerToOriginalStateWithDuration:(base::TimeDelta)duration
                                      completion:(ProceduralBlock)completion {
  [UIView animateWithDuration:duration.InSecondsF()
      animations:^{
        self.view.superview.transform = CGAffineTransformIdentity;
        [self.view.layer
            setShadowOffset:CGSizeMake(0.0, kBannerViewYShadowOffset)];
      }
      completion:^(BOOL finished) {
        if (completion)
          completion();
      }];
}

// Animate the banner back to its original position.
- (void)animateBannerToOriginalPosition {
  [UIView animateWithDuration:kSelectBannerAnimationDuration.InSecondsF()
                   animations:^{
                     self.view.center = self.originalCenter;
                   }
                   completion:nil];
}

// Animate the Banner being tapped by scaling it down and then to its original
// state. After the animation, it presents the Infobar Modal.
- (void)animateBannerTappedAndPresentModal {
  DCHECK(self.presentsModal);
  [self.interactionDelegate infobarBannerStartedInteraction];
  // TODO(crbug.com/40626691): Interrupt this animation in case the Banner needs
  // to be dismissed mid tap (Currently it will be dismmissed after the
  // animation).
  [UIView animateWithDuration:kTappedBannerAnimationDuration.InSecondsF()
      animations:^{
        self.view.superview.transform = CGAffineTransformMakeScale(
            kTappedBannerViewScale, kTappedBannerViewScale);
        [self.view.layer
            setShadowOffset:CGSizeMake(0.0, kSelectedBannerViewYShadowOffset)];
      }
      completion:^(BOOL finished) {
        [self
            animateBannerToOriginalStateWithDuration:
                kTappedBannerAnimationDuration
                                          completion:^{
                                            [self presentInfobarModalAfterTap];
                                          }];
      }];
}

- (void)presentInfobarModalAfterTap {
  DCHECK(self.presentsModal);
  base::RecordAction(base::UserMetricsAction("MobileMessagesBannerTapped"));
  [self.metricsRecorder
      recordBannerDismissType:MobileMessagesBannerDismissType::TappedToModal];
  [self recordBannerOnScreenTime];
  [self.delegate presentInfobarModalFromBanner];
}

// Records the banner on screen time. This method should be called as soon as
// its know that the banner will not be visible. This might happen before
// viewWillDissapear since presenting a Modal makes the banner invisible but
// doesn't call viewWillDissapear.
- (void)recordBannerOnScreenTime {
  if (!self.bannerOnScreenTimeWasRecorded) {
    const base::TimeDelta duration =
        base::TimeTicks::Now() - self.bannerAppearedTime;
    [self.metricsRecorder recordBannerOnScreenDuration:duration];
    self.bannerOnScreenTimeWasRecorded = YES;
  }
}

// Updates the view's shadow color when one of the view's UITraits are modified.
- (void)updateShadowColorOnTraitChange:
    (UITraitCollection*)previousTraitCollection {
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    [self.view.layer
        setShadowColor:[UIColor colorNamed:kToolbarShadowColor].CGColor];
  }
}

#pragma mark - Accessibility

- (NSArray*)accessibilityActions {
  UIAccessibilityCustomAction* acceptAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:self.buttonText
                target:self
              selector:@selector(acceptInfobar)];

  UIAccessibilityCustomAction* dismissAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_INFOBAR_BANNER_DISMISS_HINT)
                target:self
              selector:@selector(dismiss)];

  NSMutableArray* accessibilityActions =
      [@[ acceptAction, dismissAction ] mutableCopy];

  if (self.presentsModal) {
    UIAccessibilityCustomAction* modalAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:l10n_util::GetNSString(
                             IDS_IOS_INFOBAR_BANNER_OPTIONS_HINT)
                  target:self
                selector:@selector(triggerInfobarModal)];
    [accessibilityActions addObject:modalAction];
  }

  return accessibilityActions;
}

// A11y Custom actions selectors need to return a BOOL.
- (BOOL)acceptInfobar {
  [self bannerInfobarButtonWasPressed:nil];
  return NO;
}

- (BOOL)triggerInfobarModal {
  [self presentInfobarModalAfterTap];
  return NO;
}

- (BOOL)dismiss {
  [self.delegate dismissInfobarBannerForUserInteraction:YES];
  return NO;
}

- (NSString*)accessibilityLabel {
  if ([self.bannerAccessibilityLabel length])
    return self.bannerAccessibilityLabel;
  if (self.subtitleText.length) {
    return
        [NSString stringWithFormat:@"%@,%@", self.titleText, self.subtitleText];
  }
  return self.titleText;
}

@end
