// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/ui/docking_promo_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/docking_promo/ui/docking_promo_metrics.h"
#import "ios/chrome/browser/shared/ui/elements/instruction_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
constexpr CGFloat kCustomSpacingAtTopIfNoNavigationBar = 24;
constexpr CGFloat kCustomSpacingAfterImageWithoutAnimation = 0;
constexpr CGFloat kCustomSpacing = 20;
NSString* const kDarkModeAnimationSuffix = @"_darkmode";
NSString* const kEditHomeScreenKeypath = @"edit_home_screen";
NSString* const kDockingPromoAccessibilityId = @"kDockingPromoAccessibilityId";
}  // namespace

@interface DockingPromoViewController ()

// Custom animation view used in the full-screen promo.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapper;

// Custom animation view used in the full-screen promo in dark mode.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapperDarkMode;

// Child view controller used to display the alert screen for the promo.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;

// TopAnchor constraint for `alertScreen`.
@property(nonatomic, strong) NSLayoutConstraint* alertScreenTopAnchorConstraint;

// Returns true if the animationView should be displayed.
@property(nonatomic, assign, readonly) BOOL shouldShowAnimation;

@end

@implementation DockingPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kDockingPromoAccessibilityId;
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  if (self.animationViewWrapper) {
    [self configureAndLayoutAnimationView];
  }
  [self configureAlertScreen];
  [self layoutAlertScreen];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitUserInterfaceStyle.self, UITraitVerticalSizeClass.self ]);
    __weak __typeof(self) weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      [weakSelf updateUIOnTraitChange:previousCollection];
    };
    [self registerForTraitChanges:traits withHandler:handler];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
// Called when the device is rotated or dark mode is enabled/disabled. (Un)Hide
// the animations accordingly.
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateUIOnTraitChange:previousTraitCollection];
}
#endif

#pragma mark - DockingPromoConsumer

- (void)setTitleString:(NSString*)titleString
      primaryActionString:(NSString*)primaryActionString
    secondaryActionString:(NSString*)secondaryActionString
            animationName:(NSString*)animationName {
  CHECK(!self.isViewLoaded);
  CHECK(!self.alertScreen);

  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];

  alertScreen.titleString = titleString;
  alertScreen.primaryActionString = primaryActionString;
  alertScreen.secondaryActionString = secondaryActionString;
  alertScreen.actionHandler = self.actionHandler;

  _animationViewWrapper = [self createAnimation:animationName];
  _animationViewWrapperDarkMode = [self
      createAnimation:[animationName
                          stringByAppendingString:kDarkModeAnimationSuffix]];
  NSString* editHomeScreenTitle = l10n_util::GetNSString(
      IDS_IOS_DOCKING_EDIT_HOME_SCREEN_LOTTIE_INSTRUCTION);

  // Set the text localization.
  NSDictionary* textProvider = @{kEditHomeScreenKeypath : editHomeScreenTitle};
  [_animationViewWrapper setDictionaryTextProvider:textProvider];
  [_animationViewWrapperDarkMode setDictionaryTextProvider:textProvider];

  self.alertScreen = alertScreen;
}

#pragma mark - Private

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.loopAnimationCount = 1000;
  return ios::provider::GenerateLottieAnimation(config);
}

// Configures the alertScreen view.
- (void)configureAlertScreen {
  DCHECK(self.alertScreen);
  self.alertScreen.imageHasFixedSize = YES;
  self.alertScreen.showDismissBarButton = NO;
  self.alertScreen.titleTextStyle = UIFontTextStyleTitle2;
  self.alertScreen.topAlignedLayout = YES;
  self.alertScreen.customSpacing = kCustomSpacing;

  if (self.shouldShowAnimation) {
    self.alertScreen.customSpacingBeforeImageIfNoNavigationBar =
        kCustomSpacingAtTopIfNoNavigationBar;
  } else {
    self.alertScreen.customSpacingAfterImage =
        kCustomSpacingAfterImageWithoutAnimation;
  }

  // Add the Docking Promo instructional steps.
  NSArray* dockingPromoSteps = @[
    l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_FIRST_INSTRUCTION),
    l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_SECOND_INSTRUCTION),
  ];

  UIView* instructionView =
      [[InstructionView alloc] initWithList:dockingPromoSteps];

  instructionView.translatesAutoresizingMaskIntoConstraints = NO;

  self.alertScreen.underTitleView = instructionView;
  self.alertScreen.shouldFillInformationStack = YES;

  [self addChildViewController:self.alertScreen];
  [self.view addSubview:self.alertScreen.view];

  [self.alertScreen didMoveToParentViewController:self];
}

// Sets the layout of the alertScreen view.
- (void)layoutAlertScreen {
  if (self.animationViewWrapper.animationView) {
    [self layoutAlertScreenForPromoWithAnimation];
  }
}

// Sets the layout of the alertScreen view when the promo will be
// shown with the animation view (full-screen promo).
- (void)layoutAlertScreenForPromoWithAnimation {
  self.alertScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.alertScreen.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.alertScreen.view.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.alertScreen.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
  ]];
  [self updateAlertScreenTopAnchorConstraint];
}

// Updates the top anchor of the alertScreen.
// Called when the screen rotates, or in the initial layout.
- (void)updateAlertScreenTopAnchorConstraint {
  self.alertScreenTopAnchorConstraint.active = NO;
  if ([self shouldShowAnimation]) {
    self.alertScreenTopAnchorConstraint = [self.alertScreen.view.topAnchor
        constraintEqualToAnchor:self.view.centerYAnchor];
  } else {
    self.alertScreenTopAnchorConstraint = [self.alertScreen.view.topAnchor
        constraintEqualToAnchor:self.view.topAnchor];
  }
  self.alertScreenTopAnchorConstraint.active = YES;
}

// Configures the animation view and its constraints.
- (void)configureAndLayoutAnimationView {
  [self configureAndLayoutAnimationViewForWrapper:self.animationViewWrapper];
  [self configureAndLayoutAnimationViewForWrapper:
            self.animationViewWrapperDarkMode];

  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

  self.animationViewWrapper.animationView.hidden = darkModeEnabled;
  self.animationViewWrapperDarkMode.animationView.hidden = !darkModeEnabled;
  [self updateAnimationsPlaying];
}

// Helper method to configure the animation view and its constraints for the
// given LottieAnimation view.
- (void)configureAndLayoutAnimationViewForWrapper:(id<LottieAnimation>)wrapper {
  [self.view addSubview:wrapper.animationView];

  wrapper.animationView.translatesAutoresizingMaskIntoConstraints = NO;
  wrapper.animationView.contentMode = UIViewContentModeScaleAspectFit;

  [NSLayoutConstraint activateConstraints:@[
    [wrapper.animationView.leftAnchor
        constraintEqualToAnchor:self.view.leftAnchor],
    [wrapper.animationView.rightAnchor
        constraintEqualToAnchor:self.view.rightAnchor],
    [wrapper.animationView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [wrapper.animationView.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];

  [wrapper play];
}

// Returns YES if the view should display the animation view.
// The animation view should be displayed if `animationViewWrapper` is not null
// and the device is in portrait orientation.
- (BOOL)shouldShowAnimation {
  return self.animationViewWrapper.animationView &&
         self.traitCollection.verticalSizeClass !=
             UIUserInterfaceSizeClassCompact;
}

// Checks if the animations are hidden or unhidden and plays (or stops) them
// accordingly.
- (void)updateAnimationsPlaying {
  self.animationViewWrapper.animationView.hidden
      ? [self.animationViewWrapper stop]
      : [self.animationViewWrapper play];
  self.animationViewWrapperDarkMode.animationView.hidden
      ? [self.animationViewWrapperDarkMode stop]
      : [self.animationViewWrapperDarkMode play];
}

// Called when the device is rotated or dark mode is enabled/disabled. (Un)Hide
// the animations accordingly.
- (void)updateUIOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    RecordDockingPromoAction(IOSDockingPromoAction::kToggleAppearance);
  }

  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
  BOOL hidden = ![self shouldShowAnimation];

  self.animationViewWrapper.animationView.hidden = hidden || darkModeEnabled;
  self.animationViewWrapperDarkMode.animationView.hidden =
      hidden || !darkModeEnabled;

  [self updateAnimationsPlaying];
  [self updateAlertScreenTopAnchorConstraint];
}

@end
