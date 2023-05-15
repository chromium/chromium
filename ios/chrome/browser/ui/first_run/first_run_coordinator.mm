// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/first_run/default_browser/default_browser_screen_coordinator.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/history_sync/history_sync_screen_coordinator.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_coordinator.h"
#import "ios/chrome/browser/ui/first_run/tangible_sync/tangible_sync_screen_coordinator.h"
#import "ios/chrome/browser/ui/screen/screen_provider.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FirstRunCoordinator () <FirstRunScreenDelegate>

@property(nonatomic, strong) ScreenProvider* screenProvider;
@property(nonatomic, strong) ChromeCoordinator* childCoordinator;
@property(nonatomic, strong) UINavigationController* navigationController;
@property(nonatomic, strong) NSDate* firstScreenStartTime;

// YES if First Run was completed.
@property(nonatomic, assign) BOOL completed;

@end

@implementation FirstRunCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            screenProvider:(ScreenProvider*)screenProvider {
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _screenProvider = screenProvider;
    _navigationController =
        [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                      toolbarClass:nil];
    _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  }
  return self;
}

- (void)start {
  [self presentScreen:[self.screenProvider nextScreenType]];
  __weak FirstRunCoordinator* weakSelf = self;
  void (^completion)(void) = ^{
    base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kStart);
    weakSelf.firstScreenStartTime = [NSDate now];
  };
  [self.navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:self.navigationController
                                        animated:NO
                                      completion:completion];
}

- (void)stop {
  void (^completion)(void) = ^{
  };
  if (self.completed) {
    completion = ^{
      base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kComplete);
      WriteFirstRunSentinel();

      [self.delegate didFinishPresentingScreens];
    };
  }

  [self.childCoordinator stop];
  self.childCoordinator = nil;

  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:completion];
}

#pragma mark - FirstRunScreenDelegate

- (void)screenWillFinishPresenting {
  [self.childCoordinator stop];
  self.childCoordinator = nil;
  // Usually, finishing presenting the first FRE screen signifies that the user
  // has accepted Terms of Services. Therefore, we can use the time it takes the
  // first screen to be visible as the time it takes a user to accept Terms of
  // Services.
  if (self.firstScreenStartTime) {
    base::TimeDelta delta =
        base::Time::Now() - base::Time::FromNSDate(self.firstScreenStartTime);
    base::UmaHistogramTimes("FirstRun.TermsOfServicesPromoDisplayTime", delta);
    self.firstScreenStartTime = nil;
  }
  [self presentScreen:[self.screenProvider nextScreenType]];
}

- (void)skipAllScreens {
  [self.childCoordinator stop];
  self.childCoordinator = nil;
  [self willFinishPresentingScreens];
}

#pragma mark - Helper

// Presents the screen of certain `type`.
- (void)presentScreen:(ScreenType)type {
  // If no more screen need to be present, call delegate to stop presenting
  // screens.
  if (type == kStepsCompleted) {
    [self willFinishPresentingScreens];
    return;
  }
  self.childCoordinator = [self createChildCoordinatorWithScreenType:type];
  [self.childCoordinator start];
}

// Creates a screen coordinator according to `type`.
- (ChromeCoordinator*)createChildCoordinatorWithScreenType:(ScreenType)type {
  switch (type) {
    case kSignIn:
      return [[SigninScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  delegate:self
                               accessPoint:signin_metrics::AccessPoint::
                                               ACCESS_POINT_START_PAGE
                               promoAction:signin_metrics::PromoAction::
                                               PROMO_ACTION_NO_SIGNIN_PROMO];
    case kHistorySync:
      return [[HistorySyncScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  firstRun:YES
                                  delegate:self];
    case kTangibleSync:
      return [[TangibleSyncScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  firstRun:YES
                                  delegate:self];
    case kDefaultBrowserPromo:
      return [[DefaultBrowserScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  delegate:self];
    case kStepsCompleted:
      NOTREACHED() << "Reaches kStepsCompleted unexpectedly.";
      break;
  }
  return nil;
}

- (void)willFinishPresentingScreens {
  self.completed = YES;
  [self.delegate willFinishPresentingScreens];
}

@end
