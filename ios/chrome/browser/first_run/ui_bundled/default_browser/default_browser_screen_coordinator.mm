// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_mediator.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_view_controller.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"

@implementation DefaultBrowserScreenCoordinator {
  DefaultBrowserScreenViewController* _viewController;
  DefaultBrowserScreenMediator* _mediator;
  __weak id<FirstRunScreenDelegate> _delegate;
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

  ProfileIOS* profile = self.browser->GetProfile();
  base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                first_run::kDefaultBrowserScreenStart);
  default_browser::NotifyDefaultBrowserFREPromoShown(
      feature_engagement::TrackerFactory::GetForProfile(profile));

  _viewController = [[DefaultBrowserScreenViewController alloc] init];
  _viewController.delegate = self;
  _viewController.modalInPresentation = YES;

  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _viewController ]
                                           animated:animated];

  if (IsSegmentedDefaultBrowserPromoEnabled()) {
    segmentation_platform::SegmentationPlatformService* segmentationService =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetForProfile(profile);

    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForProfile(profile);

    _mediator = [[DefaultBrowserScreenMediator alloc]
           initWithSegmentationService:segmentationService
        deviceSwitcherResultDispatcher:dispatcher];
    _mediator.consumer = _viewController;
  }
}

- (void)stop {
  _viewController.delegate = nil;
  _viewController = nil;
  _delegate = nil;
  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;

  [super stop];
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
  [_delegate screenWillFinishPresenting];
}

- (void)didTapSecondaryActionButton {
  // Using `kDismiss` here instead of `kCancel` because there is no other way
  // for the user to dismiss this view as part of the FRE. `kDismiss` will not
  // cause the SetUpList Default Browser item to be marked complete.
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kDismiss);
  base::UmaHistogramEnumeration(
      first_run::kFirstRunStageHistogram,
      first_run::kDefaultBrowserScreenCompletionWithoutSettings);
  [_delegate screenWillFinishPresenting];
}

@end
