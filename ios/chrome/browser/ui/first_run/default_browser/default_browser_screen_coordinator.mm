// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/default_browser/default_browser_screen_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/first_run/default_browser/default_browser_screen_view_controller.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"

@interface DefaultBrowserScreenCoordinator () <PromoStyleViewControllerDelegate>

// Default browser screen view controller.
@property(nonatomic, strong) DefaultBrowserScreenViewController* viewController;

// First run screen delegate.
@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;

@end

@implementation DefaultBrowserScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  base::UmaHistogramEnumeration("FirstRun.Stage",
                                first_run::kDefaultBrowserScreenStart);
  [self recordDefaultBrowserPromoShown];

  self.viewController = [[DefaultBrowserScreenViewController alloc] init];
  self.viewController.delegate = self;

  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
  self.viewController.modalInPresentation = YES;
}

- (void)stop {
  self.delegate = nil;
  self.viewController = nil;
  [super stop];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  base::UmaHistogramEnumeration(
      "FirstRun.Stage", first_run::kDefaultBrowserScreenCompletionWithSettings);
  LogUserInteractionWithFirstRunPromo(YES);
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
  [self.delegate screenWillFinishPresenting];
}

- (void)didTapSecondaryActionButton {
  base::UmaHistogramEnumeration(
      "FirstRun.Stage",
      first_run::kDefaultBrowserScreenCompletionWithoutSettings);
  LogUserInteractionWithFirstRunPromo(NO);
  [self.delegate screenWillFinishPresenting];
}

#pragma mark - private

// Records that a default browser promo has been shown.
- (void)recordDefaultBrowserPromoShown {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  LogToFETDefaultBrowserPromoShown(
      feature_engagement::TrackerFactory::GetForBrowserState(browserState));
}

@end
