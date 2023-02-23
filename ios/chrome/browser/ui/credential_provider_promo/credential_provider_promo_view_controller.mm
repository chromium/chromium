// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_view_controller.h"

#import "base/values.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr CGFloat kCustomSpacingBeforeImageIfNoNavigationBar = 2;
constexpr CGFloat kCustomSpacingAfterImageWithAnimation = 24;
constexpr CGFloat kCustomSpacingAfterImageWithoutAnimation = 0;
constexpr CGFloat kPreferredCornerRadius = 20;
}  // namespace

@interface CredentialProviderPromoViewController ()

// Custom animation view used in the full-screen promo.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapper;

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
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  if (self.animationViewWrapper) {
    [self configureAndLayoutAnimationView];
  }
  [self configureAlertScreen];
  [self layoutAlertScreen];
}

// Called when the device is rotated. (Un)Hide the animation accordingly.
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  self.animationViewWrapper.animationView.hidden = ![self shouldShowAnimation];
  [self updateAlertScreenTopAnchorConstraint];
}

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

  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.loopAnimationCount = 1000;
  _animationViewWrapper = ios::provider::GenerateLottieAnimation(config);
}

#pragma mark - Private

// Configures the alertScreen view.
- (void)configureAlertScreen {
  DCHECK(self.alertScreen);
  self.alertScreen.imageHasFixedSize = YES;
  self.alertScreen.showDismissBarButton = NO;
  self.alertScreen.titleTextStyle = UIFontTextStyleTitle2;
  self.alertScreen.customSpacingBeforeImageIfNoNavigationBar =
      kCustomSpacingBeforeImageIfNoNavigationBar;
  self.alertScreen.topAlignedLayout = YES;
  self.alertScreen.customSpacingAfterImage =
      self.shouldShowAnimation ? kCustomSpacingAfterImageWithAnimation
                               : kCustomSpacingAfterImageWithoutAnimation;

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
  if (@available(iOS 15, *)) {
    self.alertScreen.modalPresentationStyle = UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        self.alertScreen.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
    presentationController.preferredCornerRadius = kPreferredCornerRadius;
  } else {
    self.alertScreen.modalPresentationStyle = UIModalPresentationFormSheet;
  }
}

// Configures the animation view and its constraints.
- (void)configureAndLayoutAnimationView {
  [self.view addSubview:self.animationViewWrapper.animationView];

  self.animationViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  self.animationViewWrapper.animationView.contentMode =
      UIViewContentModeScaleAspectFit;

  [NSLayoutConstraint activateConstraints:@[
    [self.animationViewWrapper.animationView.leftAnchor
        constraintEqualToAnchor:self.view.leftAnchor],
    [self.animationViewWrapper.animationView.rightAnchor
        constraintEqualToAnchor:self.view.rightAnchor],
    [self.animationViewWrapper.animationView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [self.animationViewWrapper.animationView.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];

  [self.animationViewWrapper play];
}

// Returns YES if the view should display the animation view.
// The animation view should be displayed if `animationView` is not null and the
// device is in portrait orientation.
- (BOOL)shouldShowAnimation {
  return self.animationViewWrapper.animationView &&
         self.traitCollection.verticalSizeClass !=
             UIUserInterfaceSizeClassCompact;
}

@end
