// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_coordinator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_mediator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"

@interface PasswordsInOtherAppsCoordinator () <
    PasswordsInOtherAppsPresenter,
    ReauthenticationCoordinatorDelegate>

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordsInOtherAppsMediator* mediator;

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordsInOtherAppsViewController* viewController;

// Used for requiring Local Authentication when the app is
// backgrounded/foregrounded with Password in Other Apps opened.
@property(nonatomic, strong) ReauthenticationCoordinator* reauthCoordinator;

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
  [self startReauthCoordinator];
}

- (void)stop {
  self.mediator = nil;
  self.viewController = nil;
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

#pragma mark - PasswordsInOtherAppsPresenter

- (void)passwordsInOtherAppsViewControllerDidDismiss {
  [self.delegate passwordsInOtherAppsCoordinatorDidRemove:self];
}

#pragma mark - ReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  // No-op.
}

- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  CHECK_EQ(_reauthCoordinator, coordinator);
  [_delegate dismissPasswordManagerAfterFailedReauthentication];
}

- (void)willPushReauthenticationViewController {
  // No-op.
}

#pragma mark - Private

// Starts reauthCoordinator. Once started
// reauthCoordinator observes scene state changes and requires authentication
// when the scene is backgrounded and then foregrounded while Password in Other
// Apps is opened.
- (void)startReauthCoordinator {
  _reauthCoordinator = [[ReauthenticationCoordinator alloc]
      initWithBaseNavigationController:_baseNavigationController
                               browser:self.browser
                reauthenticationModule:nil
                           authOnStart:NO];
  _reauthCoordinator.delegate = self;
  [_reauthCoordinator start];
}

@end
