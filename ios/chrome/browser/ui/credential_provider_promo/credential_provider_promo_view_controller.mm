// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_view_controller.h"

// TODO(crbug.com/1412808): uncomment once Lottie framework is added.
// #import <Lottie/Lottie.h>

#import "base/values.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

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
// TODO(crbug.com/1412808): replace with comment once Lottie framework is added.
// @property(nonatomic, strong) LOTAnimationView* animationView;
@property(nonatomic, strong) UIView* animationView;

// Child view controller used to display the alert screen for the half-screen
// and full-screen promos.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;

// Vertical constraint for `alertScreen` when the device is in portrait
// orientation. `alertScreen` is half the height of the screen.
@property(nonatomic, strong)
    NSLayoutConstraint* alertScreenPortraitModeVerticalConstraint;

// Vertical constraint for `alertScreen` when the device is in landscape
// orientation. `alertScreen` is the full height of the screen.
@property(nonatomic, strong)
    NSLayoutConstraint* alertScreenLandscapeModeVerticalConstraint;

// Returns true if the animationView should be displayed.
@property(nonatomic, assign, readonly) BOOL shouldShowAnimation;

@end

@implementation CredentialProviderPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  if (self.animationView) {
    [self configureAndLayoutAnimationView];
  }
  [self configureAlertScreen];
  [self layoutAlertScreen];
}

// Called when the device is rotated. In landscape orientation, `animationView`
// is hidden and `alertScreenPortraitModeVerticalConstraint` is activated. In
// portrait orientation, both `alertScreen` and `animationView` are displayed,
// and `alertScreenLandscapeModeVerticalConstraint` is activated.
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if ([self shouldShowAnimation]) {
    self.alertScreenPortraitModeVerticalConstraint.active = NO;
    self.alertScreenLandscapeModeVerticalConstraint.active = YES;
    self.animationView.hidden = NO;
  } else {
    self.alertScreenLandscapeModeVerticalConstraint.active = NO;
    self.alertScreenPortraitModeVerticalConstraint.active = YES;
    self.animationView.hidden = YES;
  }
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
  self.alertScreen = alertScreen;
}

- (void)setAnimation:(NSString*)animationAssetPath {
  DCHECK(!self.isViewLoaded);
  // TODO(crbug.com/1412808): replace line with comment once Lottie framework is
  // added.
  //_animationView = [LOTAnimationView
  // animationWithFilePath:animationAssetPath];
  _animationView = [[UIView alloc] init];
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
  if (self.animationView) {
    [self layoutAlertScreenForPromoWithAnimation];
  } else {
    [self layoutAlertScreenForPromoWithoutAnimation];
  }
}

// Sets the layout of the alertScreen view when the promo will be
// shown with the animation view (full-screen promo).
- (void)layoutAlertScreenForPromoWithAnimation {
  self.alertScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.alertScreenPortraitModeVerticalConstraint =
      [self.alertScreen.view.topAnchor
          constraintEqualToAnchor:self.view.topAnchor];
  self.alertScreenLandscapeModeVerticalConstraint =
      [self.alertScreen.view.topAnchor
          constraintEqualToAnchor:self.view.centerYAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [self.alertScreen.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.alertScreen.view.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.alertScreen.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
  ]];
  if ([self shouldShowAnimation]) {
    self.alertScreenLandscapeModeVerticalConstraint.active = YES;
  } else {
    self.alertScreenPortraitModeVerticalConstraint.active = YES;
  }
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
  [self.view addSubview:self.animationView];
  self.animationView.translatesAutoresizingMaskIntoConstraints = NO;
  self.animationView.contentMode = UIViewContentModeScaleAspectFit;
  // TODO(crbug.com/1412808): uncomment once Lottie framework is added.
  // self.animationView.loopAnimation = YES;
  //[self.animationView play];
  [NSLayoutConstraint activateConstraints:@[
    [self.animationView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.animationView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.animationView.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];
}

// Returns YES if the view should display the animation view.
// The animation view should be displayed if `animationView` is not null and the
// device is in portrait orientation.
- (BOOL)shouldShowAnimation {
  return self.animationView && self.traitCollection.verticalSizeClass !=
                                   UIUserInterfaceSizeClassCompact;
}

@end
