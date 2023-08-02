// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator.h"

#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/set_up_list_prefs.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/first_run/default_browser/default_browser_screen_view_controller.h"

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;

@implementation SetUpListDefaultBrowserPromoCoordinator {
  // The view controller that displays the default browser promo.
  DefaultBrowserScreenViewController* _viewController;

  // Application is used to open the OS settings for this app.
  UIApplication* _application;

  // Whether or not the Set Up List Item should be marked complete.
  BOOL _markItemComplete;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               application:(UIApplication*)application {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _application = application;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  RecordAction(UserMetricsAction("IOS.DefaultBrowserPromo.SetUpList.Appear"));
  [self recordDefaultBrowserPromoShown];
  _viewController = [[DefaultBrowserScreenViewController alloc] init];
  _viewController.delegate = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
  _viewController.presentationController.delegate = self;
}

- (void)stop {
  _viewController.presentationController.delegate = nil;

  ProceduralBlock completion = nil;
  if (_markItemComplete) {
    PrefService* localState = GetApplicationContext()->GetLocalState();
    completion = ^{
      set_up_list_prefs::MarkItemComplete(localState,
                                          SetUpListItemType::kDefaultBrowser);
    };
  }
  [_viewController dismissViewControllerAnimated:YES completion:completion];
  _viewController = nil;
  self.delegate = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  RecordAction(UserMetricsAction("IOS.DefaultBrowserPromo.SetUpList.Accepted"));
  [self logDefaultBrowserFullscreenPromoHistogramForAction:
            IOSDefaultBrowserPromoAction::kActionButton];
  [_application openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
  _markItemComplete = YES;
  [self.delegate setUpListDefaultBrowserPromoDidFinish:YES];
}

- (void)didTapSecondaryActionButton {
  RecordAction(UserMetricsAction("IOS.DefaultBrowserPromo.SetUpList.Dismiss"));
  [self logDefaultBrowserFullscreenPromoHistogramForAction:
            IOSDefaultBrowserPromoAction::kCancel];
  _markItemComplete = YES;
  [self.delegate setUpListDefaultBrowserPromoDidFinish:NO];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordAction(UserMetricsAction("IOS.DefaultBrowserPromo.SetUpList.Dismiss"));
  [self logDefaultBrowserFullscreenPromoHistogramForAction:
            IOSDefaultBrowserPromoAction::kCancel];
  [self.delegate setUpListDefaultBrowserPromoDidFinish:NO];
}

#pragma mark - Metrics Helpers

- (void)logDefaultBrowserFullscreenPromoHistogramForAction:
    (IOSDefaultBrowserPromoAction)action {
  UmaHistogramEnumeration("IOS.DefaultBrowserPromo.SetUpList.Action", action);
}

#pragma mark - Private

// Records that a default browser promo has been shown.
- (void)recordDefaultBrowserPromoShown {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  LogToFETDefaultBrowserPromoShown(
      feature_engagement::TrackerFactory::GetForBrowserState(browserState));
}

@end
