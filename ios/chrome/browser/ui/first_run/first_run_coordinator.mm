// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_type.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_coordinator.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_coordinator.h"
#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_coordinator.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FirstRunCoordinator () <FirstRunScreenDelegate>

@property(nonatomic, strong) FirstRunScreenProvider* screenProvider;
@property(nonatomic, strong) ChromeCoordinator* childCoordinator;
@property(nonatomic, strong) UINavigationController* navigationController;
// Whether the remaining screens have been skipped.
@property(nonatomic, assign) BOOL screensSkipped;
// Presenter for showing sync-related UI.
@property(nonatomic, readonly, weak) id<SyncPresenter> presenter;

@end

@implementation FirstRunCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                             syncPresenter:(id<SyncPresenter>)presenter
                            screenProvider:
                                (FirstRunScreenProvider*)screenProvider {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _presenter = presenter;
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
  void (^completion)(void) = ^{
    base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kStart);
  };
  [self.navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:self.navigationController
                                        animated:NO
                                      completion:completion];
}

- (void)stop {
  void (^completion)(void) = ^{
    base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kComplete);
    WriteFirstRunSentinel();
    // Display the sync errors infobar.
    web::WebState* webState =
        self.browser->GetWebStateList()->GetActiveWebState();
    DisplaySyncErrors(self.browser->GetBrowserState(), webState,
                      self.presenter);

    // If the remaining screens have been skipped, additional actions will be
    // executed.
    [self.delegate didFinishPresentingScreensWithSubsequentActionsTriggered:
                       self.screensSkipped];
  };
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:completion];
}

#pragma mark - FirstRunScreenDelegate

- (void)willFinishPresenting {
  [self.childCoordinator stop];
  self.childCoordinator = nil;
  [self presentScreen:[self.screenProvider nextScreenType]];
}

- (void)skipAll {
  [self.childCoordinator stop];
  self.childCoordinator = nil;
  self.screensSkipped = YES;
  [self.delegate willFinishPresentingScreens];
}

#pragma mark - Helper

// Presents the screen of certain |type|.
- (void)presentScreen:(FirstRunScreenType)type {
  // If no more screen need to be present, call delegate to stop presenting
  // screens.
  if (type == kFirstRunCompleted) {
    [self.delegate willFinishPresentingScreens];
    return;
  }
  self.childCoordinator = [self createChildCoordinatorWithScreenType:type];
  [self.childCoordinator start];
}

// Creates a screen coordinator according to |type|.
- (ChromeCoordinator*)createChildCoordinatorWithScreenType:
    (FirstRunScreenType)type {
  switch (type) {
    case kWelcomeAndConsent:
      return [[WelcomeScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  delegate:self];
    case kSignIn:
      return [[SigninScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  delegate:self];
    case kSync:
      return [[SyncScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  delegate:self];
    case kDefaultBrowserPromo:
      // TODO (crbug.com/1189807): Create the default browser screen.
      return nil;
    case kFirstRunCompleted:
      NOTREACHED() << "Reaches kFirstRunCompleted unexpectedly.";
      break;
  }
  return nil;
}

@end
