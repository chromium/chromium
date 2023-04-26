// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator.h"

#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/first_run/default_browser/default_browser_screen_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SetUpListDefaultBrowserPromoCoordinator {
  // The view controller that displays the default browser promo.
  DefaultBrowserScreenViewController* _viewController;

  // Application is used to open the OS settings for this app.
  UIApplication* _application;
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
  // TODO(crbug.com/1435073): Implement Set Up List metrics.
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
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  self.delegate = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/1435073): Implement Set Up List metrics.
  [_application openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
  [self.delegate setUpListDefaultBrowserPromoDidFinish:YES];
}

- (void)didTapSecondaryActionButton {
  // TODO(crbug.com/1435073): Implement Set Up List metrics.
  [self.delegate setUpListDefaultBrowserPromoDidFinish:NO];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // TODO(crbug.com/1435073): Implement Set Up List metrics.
  [self.delegate setUpListDefaultBrowserPromoDidFinish:NO];
}

#pragma mark - Private

// Records that a default browser promo has been shown.
- (void)recordDefaultBrowserPromoShown {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  LogToFETDefaultBrowserPromoShown(
      feature_engagement::TrackerFactory::GetForBrowserState(browserState));
}

@end
