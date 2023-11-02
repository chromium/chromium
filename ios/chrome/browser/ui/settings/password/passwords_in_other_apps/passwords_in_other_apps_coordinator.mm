// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/histograms.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_mediator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordsInOtherAppsCoordinator () <PasswordsInOtherAppsPresenter>

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordsInOtherAppsMediator* mediator;

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordsInOtherAppsViewController* viewController;

@end

@implementation PasswordsInOtherAppsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(browser);
    DCHECK(navigationController);
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  self.viewController = [[PasswordsInOtherAppsViewController alloc] init];
  self.viewController.presenter = self;

  self.mediator = [[PasswordsInOtherAppsMediator alloc] init];

  self.viewController.delegate = self.mediator;
  self.mediator.consumer = self.viewController;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  RecordEventOnUMA(PasswordsInOtherAppsActionOpen);
}

- (void)stop {
  self.mediator = nil;
  self.viewController = nil;
  RecordEventOnUMA(PasswordsInOtherAppsActionDismiss);
}

#pragma mark - PasswordsInOtherAppsPresenter

- (void)passwordsInOtherAppsViewControllerDidDismiss {
  [self.delegate passwordsInOtherAppsCoordinatorDidRemove:self];
}

@end
