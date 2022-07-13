// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_view_controller.h"
#import "base/check.h"
#import "base/cxx17_backports.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/ntp/incognito_view.h"
#import "ios/chrome/browser/ui/ntp/revamped_incognito_view.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Opacity of navigation bar is scroll view content offset divided by this.
const CGFloat kNavigationBarOpacityDenominator = 100;

}  // namespace

@interface IncognitoInterstitialViewController ()

// The navigation bar to display at the top of the view, to contain a "Cancel"
// button.
@property(nonatomic, strong) UINavigationBar* navigationBar;

@end

@implementation IncognitoInterstitialViewController

@dynamic delegate;

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.bannerName = @"incognito_interstitial_screen_banner";
  self.isTallBanner = YES;
  self.shouldBannerFillTopSpace = YES;

  self.titleText = l10n_util::GetNSString(IDS_IOS_INCOGNITO_INTERSTITIAL_TITLE);
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_INCOGNITO_INTERSTITIAL_OPEN_IN_CHROME_INCOGNITO);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_INTERSTITIAL_OPEN_IN_CHROME);

  UIScrollView* incognitoView = NULL;
  if (base::FeatureList::IsEnabled(kIncognitoNtpRevamp)) {
    RevampedIncognitoView* revampedIncognitoView =
        [[RevampedIncognitoView alloc] initWithFrame:CGRectZero
                       showTopIncognitoImageAndTitle:NO];
    revampedIncognitoView.URLLoaderDelegate = self.URLLoaderDelegate;
    incognitoView = revampedIncognitoView;
  } else {
    IncognitoView* revampedIncognitoView =
        [[IncognitoView alloc] initWithFrame:CGRectZero
               showTopIncognitoImageAndTitle:NO];
    revampedIncognitoView.URLLoaderDelegate = self.URLLoaderDelegate;
    incognitoView = revampedIncognitoView;
  }

  [self.specificContentView addSubview:incognitoView];

  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  self.modalInPresentation = YES;

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self.delegate
                           action:@selector(didTapCancelButton)];

  UINavigationItem* navigationRootItem =
      [[UINavigationItem alloc] initWithTitle:@""];
  navigationRootItem.rightBarButtonItem = cancelButton;

  self.navigationBar = [[UINavigationBar alloc] init];
  [self.navigationBar pushNavigationItem:navigationRootItem animated:false];
  [self updateNavigationBarAppearanceWithOpacity:0.0];

  // This needs to be called after parameters of `PromoStyleViewController` have
  // been set, but before adding additional layout constraints, since these
  // constraints can only be activated once the complete view hierarchy has been
  // constructed and relevant views belong to the same hierarchy.
  [super viewDidLoad];

  incognitoView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [incognitoView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [incognitoView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [incognitoView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [incognitoView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
    [incognitoView.heightAnchor
        constraintEqualToAnchor:incognitoView.contentLayoutGuide.heightAnchor],
  ]];

  [self.view addSubview:self.navigationBar];
  self.navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.navigationBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.navigationBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.navigationBar.topAnchor constraintEqualToAnchor:self.view.topAnchor],
  ]];
}

- (NSUInteger)supportedInterfaceOrientations {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
             ? [super supportedInterfaceOrientations]
             : UIInterfaceOrientationMaskPortrait;
}

#pragma mark - UIScrollViewDelegate

// This override allows scroll detection of the scroll view contained within the
// underlying PromoStyleViewController.
- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [super scrollViewDidScroll:scrollView];

  CGFloat navigationBarOpacity =
      scrollView.contentOffset.y / kNavigationBarOpacityDenominator;
  navigationBarOpacity =
      base::clamp(navigationBarOpacity, 0.0, 1.0, std::less_equal<>());
  [self updateNavigationBarAppearanceWithOpacity:navigationBarOpacity];
}

#pragma mark - Private

- (void)updateNavigationBarAppearanceWithOpacity:(CGFloat)opacity {
  UIColor* backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  UIColor* shadowColor = [UIColor colorNamed:kSeparatorColor];
  CGFloat backgroundAlpha = CGColorGetAlpha(backgroundColor.CGColor);
  CGFloat shadowAlpha = CGColorGetAlpha(shadowColor.CGColor);

  if (@available(iOS 15, *)) {
    UINavigationBarAppearance* appearance =
        [[UINavigationBarAppearance alloc] init];
    [appearance configureWithOpaqueBackground];
    appearance.backgroundColor =
        [backgroundColor colorWithAlphaComponent:backgroundAlpha * opacity];
    appearance.shadowColor =
        [shadowColor colorWithAlphaComponent:shadowAlpha * opacity];

    self.navigationBar.compactAppearance = appearance;
    self.navigationBar.standardAppearance = appearance;
    self.navigationBar.scrollEdgeAppearance = appearance;
  } else {
    UIImage* navigationBarBackgroundImage = ImageWithColor(
        [backgroundColor colorWithAlphaComponent:backgroundAlpha * opacity]);
    UIImage* navigationBarShadowImage = ImageWithColor(
        [shadowColor colorWithAlphaComponent:shadowAlpha * opacity]);
    [self.navigationBar setBackgroundImage:navigationBarBackgroundImage
                             forBarMetrics:UIBarMetricsDefault];

    self.navigationBar.shadowImage = navigationBarShadowImage;
  }

  self.navigationBar.tintColor = [UIColor colorNamed:kBlueColor];
}

@end
