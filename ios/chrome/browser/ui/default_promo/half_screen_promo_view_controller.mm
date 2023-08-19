// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/half_screen_promo_view_controller.h"

#import "base/values.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kCustomSpacingBeforeImageIfNoNavigationBar = 32;
constexpr CGFloat kCustomSpacingAfterImageWithoutAnimation = 24;
constexpr CGFloat kCustomFaviconSideLength = 56;
constexpr CGFloat kPreferredCornerRadius = 20;
}  // namespace

@interface HalfScreenPromoViewController ()

// Child view controller used to display the alert screen for the half-screen
// and full-screen promos.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;

@end

@implementation HalfScreenPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self addChildViewController:self.alertScreen];
  [self.view addSubview:self.alertScreen.view];
  [self.alertScreen didMoveToParentViewController:self];
  [self layoutAlertScreen];
}

#pragma mark - Private

// Configures the alertScreen view.
- (ConfirmationAlertViewController*)alertScreen {
  if (!_alertScreen) {
    _alertScreen = [[ConfirmationAlertViewController alloc] init];
    _alertScreen.titleString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_HALF_SCREEN_PROMO_TITLE_TEXT);
    _alertScreen.subtitleString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_HALF_SCREEN_PROMO_SUBTITLE_TEXT);
    _alertScreen.primaryActionString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_HALF_SCREEN_PROMO_PRIMARY_BUTTON_TEXT);
    _alertScreen.secondaryActionString = l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_HALF_SCREEN_PROMO_SECONDARY_BUTTON_TEXT);
    _alertScreen.image = ios::provider::GetBrandedImage(
        ios::provider::BrandedImage::kNonModalDefaultBrowserPromo);
    _alertScreen.actionHandler = self.actionHandler;
    _alertScreen.imageHasFixedSize = YES;
    _alertScreen.showDismissBarButton = NO;
    _alertScreen.titleTextStyle = UIFontTextStyleTitle2;
    _alertScreen.customSpacingBeforeImageIfNoNavigationBar =
        kCustomSpacingBeforeImageIfNoNavigationBar;
    _alertScreen.topAlignedLayout = YES;
    _alertScreen.imageEnclosedWithShadowWithoutBadge = YES;
    _alertScreen.customFaviconSideLength = kCustomFaviconSideLength;
    _alertScreen.customSpacingAfterImage =
        kCustomSpacingAfterImageWithoutAnimation;
  }

  return _alertScreen;
}

// Sets the layout of the alertScreen view when the promo will be
// shown without the animation view (half-screen promo).
- (void)layoutAlertScreen {
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

@end
