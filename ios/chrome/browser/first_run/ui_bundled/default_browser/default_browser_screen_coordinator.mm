// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_coordinator.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_animated_screen_view_controller.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_mediator.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_view_controller.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_provider.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/tos/tos_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/uma/uma_coordinator.h"
#import "ios/chrome/browser/instructions_bottom_sheet/ui/instructions_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface DefaultBrowserScreenCoordinator () <TOSCoordinatorDelegate,
                                               UMACoordinatorDelegate>
@end

@implementation DefaultBrowserScreenCoordinator {
  // The Default Browser Promo can be displayed as either a Static promo or an
  // Animated promo depending on the `kAnimatedDefaultBrowserPromoInFRE` flag.
  // Seperate view controllers are used to create each view and set the
  // necessary properties, but only one view is presented.
  DefaultBrowserScreenViewController* _staticViewController;
  DefaultBrowserAnimatedScreenViewController* _animatedViewController;
  InstructionsBottomSheetCoordinator* _instructionsCoordinator;
  DefaultBrowserScreenMediator* _mediator;
  __weak id<FirstRunScreenDelegate> _delegate;
  TOSCoordinator* _TOSCoordinator;
  UMACoordinator* _UMACoordinator;
  raw_ptr<ProfileIOS> _profile;
}
@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  _profile = self.profile->GetOriginalProfile();
  base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                first_run::kDefaultBrowserScreenStart);
  default_browser::NotifyDefaultBrowserFREPromoShown(
      feature_engagement::TrackerFactory::GetForProfile(_profile));

  if (first_run::IsAnimatedDefaultBrowserPromoInFREEnabled()) {
    [self displayAnimatedPromo];
  } else {
    [self displayStaticPromo];
  }
}

- (void)stop {
  _animatedViewController = nil;
  _staticViewController.delegate = nil;
  _staticViewController = nil;
  _delegate = nil;
  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;
  _instructionsCoordinator = nil;
  [self stopTOSCoordinator];

  [super stop];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kActionButton);
  base::UmaHistogramEnumeration(
      first_run::kFirstRunStageHistogram,
      first_run::kDefaultBrowserScreenCompletionWithSettings);

  OpenIOSDefaultBrowserSettingsPage();

  [self finishPresenting];
}

- (void)didTapSecondaryActionButton {
  // Using `kDismiss` here instead of `kCancel` because there is no other way
  // for the user to dismiss this view as part of the FRE. `kDismiss` will not
  // cause the SetUpList Default Browser item to be marked complete.
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kDismiss);
  base::UmaHistogramEnumeration(
      first_run::kFirstRunStageHistogram,
      first_run::kDefaultBrowserScreenCompletionWithoutSettings);
  [self finishPresenting];
}

- (void)didTapTertiaryActionButton {
  if (first_run::AnimatedDefaultBrowserPromoInFREExperimentTypeEnabled() ==
      first_run::AnimatedDefaultBrowserPromoInFREExperimentType::
          kAnimationWithShowMeHow) {
    NSMutableArray* defaultBrowserSteps = [[NSMutableArray alloc] init];
    if (IsDefaultAppsDestinationAvailable() &&
        IsUseDefaultAppsDestinationForPromosEnabled()) {
      [defaultBrowserSteps
          addObject:
              l10n_util::GetNSString(
                  IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_DEFAULT_APPS_FIRST_STEP)];
      [defaultBrowserSteps
          addObject:
              l10n_util::GetNSString(
                  IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_DEFAULT_APPS_SECOND_STEP)];
    } else {
      [defaultBrowserSteps
          addObject:l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_FIRST_STEP)];
      [defaultBrowserSteps
          addObject:l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECOND_STEP)];
    }
    [defaultBrowserSteps
        addObject:l10n_util::GetNSString(
                      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_THIRD_STEP)];
    _instructionsCoordinator = [[InstructionsBottomSheetCoordinator alloc]
        initWithBaseViewController:_animatedViewController
                           browser:self.browser
                             title:nil
                             steps:defaultBrowserSteps];

    [_instructionsCoordinator start];
  }
}

- (void)didTapURLInDisclaimer:(NSURL*)URL {
  if ([URL.absoluteString isEqualToString:first_run::kTermsOfServiceURL]) {
    [self showTOSPage];
  } else if ([URL.absoluteString
                 isEqualToString:first_run::kMetricReportingURL]) {
    _mediator.UMALinkWasTapped = YES;
    [self showUMADialog];
  } else {
    NOTREACHED() << std::string("Unknown URL ")
                 << base::SysNSStringToUTF8(URL.absoluteString);
  }
}

#pragma mark - TOSCoordinatorDelegate

- (void)TOSCoordinatorWantsToBeStopped:(TOSCoordinator*)coordinator {
  CHECK_EQ(_TOSCoordinator, coordinator, base::NotFatalUntil::M144);
  [self stopTOSCoordinator];
}

#pragma mark - UMACoordinatorDelegate

- (void)UMACoordinatorDidRemoveWithCoordinator:(UMACoordinator*)coordinator
                        UMAReportingUserChoice:(BOOL)UMAReportingUserChoice {
  DCHECK(_UMACoordinator);
  DCHECK_EQ(_UMACoordinator, coordinator);
  [self stopUMACoordinator];
  DCHECK(_mediator);
  _mediator.UMAReportingUserChoice = UMAReportingUserChoice;
}

#pragma mark - Private

- (void)stopTOSCoordinator {
  [_TOSCoordinator stop];
  _TOSCoordinator.delegate = nil;
  _TOSCoordinator = nil;
}

- (void)showTOSPage {
  DCHECK(!_TOSCoordinator);
  CHECK(_staticViewController);
  _mediator.TOSLinkWasTapped = YES;
  _TOSCoordinator =
      [[TOSCoordinator alloc] initWithBaseViewController:_staticViewController
                                                 browser:self.browser];
  _TOSCoordinator.delegate = self;
  [_TOSCoordinator start];
}

- (void)displayStaticPromo {
  _staticViewController = [[DefaultBrowserScreenViewController alloc] init];
  _staticViewController.delegate = self;

  if (base::FeatureList::IsEnabled(first_run::kUpdatedFirstRunSequence)) {
    _mediator = [[DefaultBrowserScreenMediator alloc] init];

    _mediator.consumer = _staticViewController;
  }

  BOOL animated = self.baseNavigationController.topViewController != nil;
  _staticViewController.delegate = self;
  _staticViewController.modalInPresentation = YES;
  [self.baseNavigationController setViewControllers:@[ _staticViewController ]
                                           animated:animated];
}

- (void)displayAnimatedPromo {
  _animatedViewController =
      [[DefaultBrowserAnimatedScreenViewController alloc] init];
  _animatedViewController.shouldHideBanner = YES;

  BOOL animated = self.baseNavigationController.topViewController != nil;
  _animatedViewController.delegate = self;
  _animatedViewController.modalInPresentation = YES;
  [self.baseNavigationController setViewControllers:@[ _animatedViewController ]
                                           animated:animated];
}

- (void)finishPresenting {
  [_mediator finishPresenting];
  [_delegate screenWillFinishPresenting];
}

// Shows the UMA dialog so the user can manage metric reporting.
- (void)showUMADialog {
  DCHECK(!_UMACoordinator);
  CHECK(_staticViewController);
  _UMACoordinator = [[UMACoordinator alloc]
      initWithBaseViewController:_staticViewController
                         browser:self.browser
               UMAReportingValue:_mediator.UMAReportingUserChoice];
  _UMACoordinator.delegate = self;
  [_UMACoordinator start];
}

- (void)stopUMACoordinator {
  [_UMACoordinator stop];
  _UMACoordinator.delegate = nil;
  _UMACoordinator = nil;
}

@end
