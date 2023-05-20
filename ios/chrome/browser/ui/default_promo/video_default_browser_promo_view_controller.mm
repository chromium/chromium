// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_view_controller.h"

#import "base/values.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr CGFloat kSpacingBeforeImageIfNoNavigationBar = 2;
constexpr CGFloat kpacingAfterImageWithAnimation = 24;
NSString* const kDarkModeAnimationSuffix = @"_darkmode";
NSString* const kDefaultBrowserAnimation = @"default_browser_animation";
}  // namespace

@interface VideoDefaultBrowserPromoViewController ()

// Custom animation view used in the full-screen promo.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapper;
// Custom animation view used in the full-screen promo in dark mode.
@property(nonatomic, strong) id<LottieAnimation> animationViewWrapperDarkMode;
// Child view controller used to display the alert screen for the half-screen
// and full-screen promos.
@property(nonatomic, strong) ConfirmationAlertViewController* alertScreen;

// Properties set on initialization.
@property(nonatomic, copy) NSString* titleText;
@property(nonatomic, copy) NSString* subtitle;
@property(nonatomic, copy) NSString* primaryActionTitle;
@property(nonatomic, copy) NSString* secondaryActionTitle;
@property(nonatomic, copy) NSString* animationAssetName;

@end

@implementation VideoDefaultBrowserPromoViewController

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _animationViewWrapper = [self createAnimation:kDefaultBrowserAnimation];
    _animationViewWrapperDarkMode = [self
        createAnimation:[kDefaultBrowserAnimation
                            stringByAppendingString:kDarkModeAnimationSuffix]];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kGrey100Color];
  [self
      createConfirmationAlertScreen:
          l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_TITLE_TEXT)
                     subtitleString:
                         l10n_util::GetNSString(
                             IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_SUBTITLE_TEXT)
                primaryActionString:
                    l10n_util::GetNSString(
                        IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_PRIMARY_BUTTON_TEXT)
              secondaryActionString:
                  l10n_util::GetNSString(
                      IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_SECONDARY_BUTTON_TEXT)];

  [self configureAnimationView];
  [self configureAlertScreen];
  [self layoutAlertScreen];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

  self.animationViewWrapper.animationView.hidden = darkModeEnabled;
  self.animationViewWrapperDarkMode.animationView.hidden = !darkModeEnabled;

  [self updateAnimationsPlaying];
}

#pragma mark - Private

// Creates a confirmation alert view controller and sets the strings.
- (void)createConfirmationAlertScreen:(NSString*)titleString
                       subtitleString:(NSString*)subtitleString
                  primaryActionString:(NSString*)primaryActionString
                secondaryActionString:(NSString*)secondaryActionString {
  DCHECK(!self.alertScreen);
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  alertScreen.titleString = titleString;
  alertScreen.subtitleString = subtitleString;
  alertScreen.primaryActionString = primaryActionString;
  alertScreen.secondaryActionString = secondaryActionString;
  alertScreen.actionHandler = self.actionHandler;
  self.alertScreen = alertScreen;
}

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
  self.alertScreen.customSpacingBeforeImageIfNoNavigationBar =
      kSpacingBeforeImageIfNoNavigationBar;
  self.alertScreen.topAlignedLayout = YES;
  self.alertScreen.customSpacingAfterImage = kpacingAfterImageWithAnimation;

  [self addChildViewController:self.alertScreen];
  [self.view addSubview:self.alertScreen.view];

  [self.alertScreen didMoveToParentViewController:self];
}

// Sets the layout of the alertScreen view.
- (void)layoutAlertScreen {
  self.alertScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.alertScreen.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.alertScreen.view.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.alertScreen.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
    [self.alertScreen.view.topAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];
}

// Configures the animation view and its constraints.
- (void)configureAnimationView {
  [self.view addSubview:self.animationViewWrapper.animationView];
  [self.view addSubview:self.animationViewWrapperDarkMode.animationView];

  self.animationViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  self.animationViewWrapper.animationView.contentMode =
      UIViewContentModeScaleAspectFit;
  self.animationViewWrapperDarkMode.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  self.animationViewWrapperDarkMode.animationView.contentMode =
      UIViewContentModeScaleAspectFit;

  [NSLayoutConstraint activateConstraints:@[
    [self.animationViewWrapper.animationView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.animationViewWrapper.animationView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.animationViewWrapper.animationView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [self.animationViewWrapper.animationView.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];

  AddSameConstraints(self.animationViewWrapperDarkMode.animationView,
                     self.animationViewWrapper.animationView);

  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

  self.animationViewWrapper.animationView.hidden = darkModeEnabled;
  self.animationViewWrapperDarkMode.animationView.hidden = !darkModeEnabled;
  [self updateAnimationsPlaying];
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

@end
