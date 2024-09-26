// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_view_controller.h"

#import "base/values.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kCustomSpacingAtTopIfNoNavigationBar = 24;
constexpr CGFloat kCustomSpacingAfterImageWithoutAnimation = 0;
constexpr CGFloat kPreferredCornerRadius = 20;
NSString* const kDarkModeAnimationSuffix = @"_darkmode";
NSString* const kPasswordOptionsKeypath = @"text_password_options";
NSString* const kCredentialProviderPromoAccessibilityId =
    @"kCredentialProviderPromoAccessibilityId";
}  // namespace

@interface CredentialProviderPromoViewController ()

// Custom animation view used in the full-screen promo.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapper;

// Custom animation view used in the full-screen promo in dark mode.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapperDarkMode;

// Child view controller used to display the alert screen for the half-screen
// and full-screen promos.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;

// TopAnchor constraint for `alertScreen`.
@property(nonatomic, strong) NSLayoutConstraint* alertScreenTopAnchorConstraint;

// Returns true if the animationView should be displayed.
@property(nonatomic, assign, readonly) BOOL shouldShowAnimation;

@end

@implementation CredentialProviderPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kCredentialProviderPromoAccessibilityId;
  self.view.backgroundColor = [UIColor colorNamed:kGrey100Color];
  if (self.animationViewWrapper) {
    [self configureAndLayoutAnimationView];
  }
  [self configureAlertScreen];
  [self layoutAlertScreen];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitVerticalSizeClass.self, UITraitUserInterfaceStyle.self ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(updateUIOnTraitChange)];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateUIOnTraitChange];
}
#endif

#pragma mark - CredentialProviderPromoConsumer

- (void)setTitleString:(NSString*)titleString
           subtitleString:(NSString*)subtitleString
      primaryActionString:(NSString*)primaryActionString
    secondaryActionString:(NSString*)secondaryActionString
     tertiaryActionString:(NSString*)tertiaryActionString
                    image:(UIImage*)image {
  DCHECK(!self.alertScreen);
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  alertScreen.titleString = titleString;
  alertScreen.subtitleString = subtitleString;
  alertScreen.primaryActionString = primaryActionString;
  alertScreen.secondaryActionString = secondaryActionString;
  alertScreen.tertiaryActionString = tertiaryActionString;
  alertScreen.image = image;
  alertScreen.actionHandler = self.actionHandler;
  self.alertScreen = alertScreen;
}

- (void)setAnimation:(NSString*)animationAssetName {
  DCHECK(!self.isViewLoaded);

  _animationViewWrapper = [self createAnimation:animationAssetName];
  _animationViewWrapperDarkMode = [self
      createAnimation:[animationAssetName
                          stringByAppendingString:kDarkModeAnimationSuffix]];

  NSString* passwordSettingsTitle;
  passwordSettingsTitle = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_PROVIDER_PROMO_OS_PASSWORDS_SETTINGS_TITLE_IOS16);
  // Set the text localization.
  NSDictionary* textProvider =
      @{kPasswordOptionsKeypath : passwordSettingsTitle};
  [_animationViewWrapper setDictionaryTextProvider:textProvider];
  [_animationViewWrapperDarkMode setDictionaryTextProvider:textProvider];
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

  if (self.shouldShowAnimation) {
    self.alertScreen.customSpacingBeforeImageIfNoNavigationBar =
        kCustomSpacingAtTopIfNoNavigationBar;
  } else {
    self.alertScreen.customSpacingAfterImage =
        kCustomSpacingAfterImageWithoutAnimation;
  }

  [self addChildViewController:self.alertScreen];
  [self.view addSubview:self.alertScreen.view];

  [self.alertScreen didMoveToParentViewController:self];
}

// Sets the layout of the alertScreen view.
- (void)layoutAlertScreen {
  if (self.animationViewWrapper.animationView) {
    [self layoutAlertScreenForPromoWithAnimation];
  } else {
    [self layoutAlertScreenForPromoWithoutAnimation];
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

// Sets the layout of the alertScreen view when the promo will be
// shown without the animation view (half-screen promo).
- (void)layoutAlertScreenForPromoWithoutAnimation {
  self.alertScreen.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.alertScreen.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  presentationController.preferredCornerRadius = kPreferredCornerRadius;
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
- (void)updateUIOnTraitChange {
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
