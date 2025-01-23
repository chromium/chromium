// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_coordinator.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_mediator.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_view_controller.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_provider.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/tos/tos_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/uma/uma_coordinator.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/tos_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface DefaultBrowserScreenCoordinator () <TOSCommands,
                                               UMACoordinatorDelegate,
                                               ConfirmationAlertActionHandler>
@end

@implementation DefaultBrowserScreenCoordinator {
  // The Default Browser Promo can be displayed as either a Static promo or an
  // Animated promo depending on the `kAnimatedDefaultBrowserPromoInFRE` flag.
  // Seperate view controllers are used to create each view and set the
  // necessary properties, but only one view is presented.
  DefaultBrowserScreenViewController* _staticViewController;
  DefaultBrowserInstructionsViewController* _animatedViewController;
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

  _profile = self.browser->GetProfile()->GetOriginalProfile();
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

  [super stop];
}
#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self didTapPrimaryActionButton];
}

- (void)confirmationAlertSecondaryAction {
  [self didTapSecondaryActionButton];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kActionButton);
  base::UmaHistogramEnumeration(
      first_run::kFirstRunStageHistogram,
      first_run::kDefaultBrowserScreenCompletionWithSettings);
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
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

#pragma mark - TOSCommands

- (void)showTOSPage {
  DCHECK(!_TOSCoordinator);
  CHECK(_staticViewController);
  _mediator.TOSLinkWasTapped = YES;
  _TOSCoordinator =
      [[TOSCoordinator alloc] initWithBaseViewController:_staticViewController
                                                 browser:self.browser];
  [_TOSCoordinator start];
}

- (void)closeTOSPage {
  DCHECK(_TOSCoordinator);
  [_TOSCoordinator stop];
  _TOSCoordinator = nil;
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

- (void)displayStaticPromo {
  _staticViewController = [[DefaultBrowserScreenViewController alloc] init];
  _staticViewController.delegate = self;

  if (IsSegmentedDefaultBrowserPromoEnabled() ||
      base::FeatureList::IsEnabled(first_run::kUpdatedFirstRunSequence)) {
    segmentation_platform::SegmentationPlatformService* segmentationService =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetForProfile(_profile);

    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForProfile(_profile);

    _mediator = [[DefaultBrowserScreenMediator alloc]
           initWithSegmentationService:segmentationService
        deviceSwitcherResultDispatcher:dispatcher];

    _mediator.consumer = _staticViewController;
  }
  BOOL animated = self.baseNavigationController.topViewController != nil;
  _staticViewController.delegate = self;
  _staticViewController.modalInPresentation = YES;
  [self.baseNavigationController setViewControllers:@[ _staticViewController ]
                                           animated:animated];
}

- (void)displayAnimatedPromo {
  _animatedViewController = [[DefaultBrowserInstructionsViewController alloc]
      initWithDismissButton:YES
           hasRemindMeLater:NO
                   hasSteps:NO
              actionHandler:self
                  titleText:nil];

  BOOL animated = self.baseNavigationController.topViewController != nil;
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
