// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/default_browser/coordinator/default_browser_screen_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/coordinator/first_run_screen_provider.h"
#import "ios/chrome/browser/first_run/default_browser/coordinator/default_browser_screen_mediator.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_android_switcher_screen_view_controller.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_animated_screen_view_controller.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_screen_consumer.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_screen_view_controller.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/public/first_run_util.h"
#import "ios/chrome/browser/first_run/tos/coordinator/tos_coordinator.h"
#import "ios/chrome/browser/first_run/uma/coordinator/uma_coordinator.h"
#import "ios/chrome/browser/instructions_bottom_sheet/ui/instructions_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface DefaultBrowserScreenCoordinator () <
    DefaultBrowserScreenMediatorDelegate,
    TOSCoordinatorDelegate,
    UMACoordinatorDelegate>
@end

@implementation DefaultBrowserScreenCoordinator {
  PromoStyleViewController<DefaultBrowserScreenConsumer>* _viewController;
  InstructionsBottomSheetCoordinator* _instructionsCoordinator;
  DefaultBrowserScreenMediator* _mediator;
  __weak id<FirstRunScreenDelegate> _delegate;
  TOSCoordinator* _TOSCoordinator;
  UMACoordinator* _UMACoordinator;
  raw_ptr<ProfileIOS> _profile;
  UIView* _transparentView;
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

- (void)dealloc {
  CHECK(!_instructionsCoordinator, base::NotFatalUntil::M155);
  CHECK(!_mediator, base::NotFatalUntil::M155);
  CHECK(!_viewController, base::NotFatalUntil::M155);
  CHECK(!_transparentView);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  _profile = self.profile->GetOriginalProfile();
  _mediator = [[DefaultBrowserScreenMediator alloc]
             initWithIdentityManager:IdentityManagerFactory::GetForProfile(
                                         _profile)
               authenticationService:AuthenticationServiceFactory::
                                         GetForProfile(_profile)
      deviceSwitcherResultDispatcher:segmentation_platform::
                                         SegmentationPlatformServiceFactory::
                                             GetDispatcherForProfile(_profile)];
  _mediator.delegate = self;
  [_mediator startPromoEvaluation];
  if (!_viewController) {
    [self showTransparentOverlay];
  }
}

- (void)stop {
  [_transparentView removeFromSuperview];
  _transparentView = nil;
  _viewController.delegate = nil;
  _viewController = nil;
  _delegate = nil;
  _mediator.consumer = nil;
  _mediator.delegate = nil;
  [_mediator disconnect];
  _mediator = nil;
  _instructionsCoordinator = nil;
  [self stopTOSCoordinator];
  _profile = nullptr;

  [super stop];
}

#pragma mark - DefaultBrowserScreenMediatorDelegate

- (void)defaultBrowserScreenMediator:(DefaultBrowserScreenMediator*)mediator
    didFinishEvaluationWithAndroidSwitcherPromo:(BOOL)showAndroidSwitcherPromo {
  CHECK_EQ(_mediator, mediator);
  [self presentScreenWithAndroidSwitcherPromo:showAndroidSwitcherPromo];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kActionButton,
      IOSDefaultBrowserPromoSurface::kFirstRun);
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
        initWithBaseViewController:_viewController
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
  CHECK_EQ(_TOSCoordinator, coordinator);
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

// Presents the Default Browser promo screen.
- (void)presentScreenWithAndroidSwitcherPromo:(BOOL)showAndroidSwitcherPromo {
  [_transparentView removeFromSuperview];
  _transparentView = nil;

  base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                first_run::kDefaultBrowserScreenStart);
  default_browser::NotifyDefaultBrowserFREPromoShown(
      feature_engagement::TrackerFactory::GetForProfile(_profile));

  _viewController = [self
      createViewControllerWithAndroidSwitcherPromo:showAndroidSwitcherPromo];
  _viewController.delegate = self;
  _viewController.modalInPresentation = YES;
  _mediator.consumer = _viewController;

  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _viewController ]
                                           animated:animated];
}

- (void)stopTOSCoordinator {
  [_TOSCoordinator stop];
  _TOSCoordinator.delegate = nil;
  _TOSCoordinator = nil;
}

- (void)showTOSPage {
  DCHECK(!_TOSCoordinator);
  CHECK(_viewController);
  _mediator.TOSLinkWasTapped = YES;
  _TOSCoordinator =
      [[TOSCoordinator alloc] initWithBaseViewController:_viewController
                                                 browser:self.browser];
  _TOSCoordinator.delegate = self;
  [_TOSCoordinator start];
}

// Shows a transparent overlay on `baseViewController` to block user interaction
// while waiting for the device switcher signal.
- (void)showTransparentOverlay {
  _transparentView =
      [[UIView alloc] initWithFrame:self.baseViewController.view.bounds];
  _transparentView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  _transparentView.backgroundColor = [UIColor clearColor];
  [self.baseViewController.view addSubview:_transparentView];
}

// Creates and configures the promo view controller to present.
- (PromoStyleViewController<DefaultBrowserScreenConsumer>*)
    createViewControllerWithAndroidSwitcherPromo:
        (BOOL)showAndroidSwitcherPromo {
  if (first_run::IsAnimatedDefaultBrowserPromoInFREEnabled()) {
    DefaultBrowserAnimatedScreenViewController* viewController =
        [[DefaultBrowserAnimatedScreenViewController alloc] init];
    viewController.shouldHideBanner = YES;
    return viewController;
  }
  if (showAndroidSwitcherPromo) {
    return [[DefaultBrowserAndroidSwitcherScreenViewController alloc] init];
  }
  return [[DefaultBrowserScreenViewController alloc] init];
}

- (void)finishPresenting {
  [_mediator finishPresenting];
  [_delegate firstRunScreenCoordinatorWantsToBeStopped:self];
}

// Shows the UMA dialog so the user can manage metric reporting.
- (void)showUMADialog {
  DCHECK(!_UMACoordinator);
  CHECK(_viewController);
  _UMACoordinator = [[UMACoordinator alloc]
      initWithBaseViewController:_viewController
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
