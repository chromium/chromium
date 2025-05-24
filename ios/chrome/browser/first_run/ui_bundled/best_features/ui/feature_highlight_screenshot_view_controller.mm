// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/feature_highlight_screenshot_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/metrics_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
NSString* const kFeatureHighlightScreenshotViewAccessibilityIdentifier =
    @"FeatureHighlightScreenshotViewAccessibilityIdentifier";
NSString* const kFeatureHighlightScreenshotViewAnimationId =
    @"FeatureHighlightScreenshotViewAnimationId";
NSString* const kFeatureHighlightScreenshotViewDarkAnimationId =
    @"FeatureHighlightScreenshotViewDarkAnimationId";
}  // namespace

@interface FeatureHighlightScreenshotViewController () <
    UINavigationControllerDelegate>

@end

@implementation FeatureHighlightScreenshotViewController {
  // The item that the promo is configured to present.
  BestFeaturesItem* _bestFeaturesItem;
  // The action handler for the view controller;
  id<ConfirmationAlertActionHandler> _actionHandler;
  // The screenshot view.
  id<LottieAnimation> _screenshotViewWrapper;
  // The screenshot view in dark mode.
  id<LottieAnimation> _screenshotViewWrapperDarkMode;
}

- (instancetype)initWithFeatureHighlightItem:(BestFeaturesItem*)bestFeaturesItem
                               actionHandler:
                                   (id<ConfirmationAlertActionHandler>)
                                       actionHandler {
  self = [super init];
  if (self) {
    _bestFeaturesItem = bestFeaturesItem;
    _actionHandler = actionHandler;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.class ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(toggleDarkModeOnTraitChange)];
  }

  [self.view setBackgroundColor:[UIColor colorNamed:kSecondaryBackgroundColor]];
  self.accessibilityLabel =
      kFeatureHighlightScreenshotViewAccessibilityIdentifier;

  [self showPromo];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  if (self.isMovingFromParentViewController) {
    // Back button was pressed.
    base::UmaHistogramEnumeration(
        BestFeaturesActionHistogramForItemType(_bestFeaturesItem.type),
        BestFeaturesDetailScreenActionType::kNavigateBack);
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    [self toggleDarkModeOnTraitChange];
  }
}
#endif

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  [navigationController setNavigationBarHidden:(viewController != self)];
}

#pragma mark - Private

// Creates and displays the ConfirmationAlertScreen on the bottom half of the
// view and an animation on the top half of the view.
- (void)showPromo {
  [self addAnimation];

  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  alertScreen.actionHandler = _actionHandler;
  alertScreen.titleString = _bestFeaturesItem.title;
  alertScreen.titleTextStyle = UIFontTextStyleTitle2;
  alertScreen.subtitleString = _bestFeaturesItem.subtitle;
  alertScreen.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_SHOW_ME_HOW_FIRST_RUN_TITLE);

  // The secondary action string changes depending on the FRE Sequence.
  if ([self isBestFeaturesLast]) {
    alertScreen.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_START_BROWSING_BUTTON);
  } else {
    alertScreen.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_CONTINUE_BUTTON);
  }
  alertScreen.imageHasFixedSize = YES;
  alertScreen.topAlignedLayout = YES;
  alertScreen.showDismissBarButton = NO;
  alertScreen.customSpacingAfterImage = 24;
  alertScreen.customSpacingBeforeImageIfNoNavigationBar = 24;

  [self addChildViewController:alertScreen];
  [self.view addSubview:alertScreen.view];
  [alertScreen didMoveToParentViewController:self];

  // Layout the alert view to take up bottom half of the view.
  alertScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [alertScreen.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [alertScreen.view.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [alertScreen.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
    [alertScreen.view.topAnchor constraintEqualToAnchor:self.view.centerYAnchor
                                               constant:35],
  ]];
}

// Adds an animation to the top half of the view.
- (void)addAnimation {
  _screenshotViewWrapper =
      [self createAnimation:_bestFeaturesItem.animationName];
  _screenshotViewWrapperDarkMode =
      [self createAnimation:[_bestFeaturesItem.animationName
                                stringByAppendingString:@"_darkmode"]];

  // Set the text localization for the screenshot.
  [_screenshotViewWrapper
      setDictionaryTextProvider:_bestFeaturesItem.textProvider];
  [_screenshotViewWrapperDarkMode
      setDictionaryTextProvider:_bestFeaturesItem.textProvider];

  [self.view addSubview:_screenshotViewWrapper.animationView];
  [self.view addSubview:_screenshotViewWrapperDarkMode.animationView];

  // Layout the animation view to take up the top half of the view.
  _screenshotViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  _screenshotViewWrapper.animationView.contentMode =
      UIViewContentModeScaleAspectFit;
  _screenshotViewWrapper.animationView.accessibilityIdentifier =
      kFeatureHighlightScreenshotViewAnimationId;
  _screenshotViewWrapperDarkMode.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  _screenshotViewWrapperDarkMode.animationView.contentMode =
      UIViewContentModeScaleAspectFit;
  _screenshotViewWrapper.animationView.accessibilityIdentifier =
      kFeatureHighlightScreenshotViewDarkAnimationId;

  [NSLayoutConstraint activateConstraints:@[
    [_screenshotViewWrapper.animationView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_screenshotViewWrapper.animationView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_screenshotViewWrapper.animationView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:29],
    [_screenshotViewWrapper.animationView.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor
                       constant:35],
  ]];

  AddSameConstraints(_screenshotViewWrapperDarkMode.animationView,
                     _screenshotViewWrapper.animationView);

  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

  _screenshotViewWrapper.animationView.hidden = darkModeEnabled;
  _screenshotViewWrapperDarkMode.animationView.hidden = !darkModeEnabled;

  [self updateAnimationsPlaying];
}

// Creates an animation with the given animationAssetName and sets it to always
// loop (if applicable).
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.loopAnimationCount = -1;
  return ios::provider::GenerateLottieAnimation(config);
}

// Checks if the animations are hidden or unhidden and plays (or stops) them
// accordingly.
- (void)updateAnimationsPlaying {
  _screenshotViewWrapper.animationView.hidden ? [_screenshotViewWrapper stop]
                                              : [_screenshotViewWrapper play];
  _screenshotViewWrapperDarkMode.animationView.hidden
      ? [_screenshotViewWrapperDarkMode stop]
      : [_screenshotViewWrapperDarkMode play];
}

// Toggle dark mode view when UITraitUserInterfaceStyle is changed on device.
- (void)toggleDarkModeOnTraitChange {
  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

  _screenshotViewWrapper.animationView.hidden = darkModeEnabled;
  _screenshotViewWrapperDarkMode.animationView.hidden = !darkModeEnabled;
}

// Returns 'YES' if the Best Features screen is last in the FRE sequence.
- (BOOL)isBestFeaturesLast {
  using enum first_run::BestFeaturesScreenVariationType;
  switch (first_run::GetBestFeaturesScreenVariationType()) {
    case kGeneralScreenAfterDBPromo:
    case kGeneralScreenWithPasswordItemAfterDBPromo:
    case kShoppingUsersWithFallbackAfterDBPromo:
    case kSignedInUsersOnlyAfterDBPromo:
      return YES;
    case kGeneralScreenBeforeDBPromo:
      return NO;
    case kAddressBarPromoInsteadOfBestFeaturesScreen:
    case kDisabled:
      NOTREACHED();
  }
}

@end
