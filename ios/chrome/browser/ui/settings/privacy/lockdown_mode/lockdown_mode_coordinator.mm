// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_coordinator.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_mediator.h"
#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_view_controller.h"

@interface LockdownModeCoordinator () <
    LockdownModeViewControllerPresentationDelegate>

@property(nonatomic, strong) LockdownModeViewController* viewController;
@property(nonatomic, strong) LockdownModeMediator* mediator;

@end

@implementation LockdownModeCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  self.viewController =
      [[LockdownModeViewController alloc] initWithStyle:ChromeTableViewStyle()];
  self.viewController.presentationDelegate = self;
  self.mediator = [[LockdownModeMediator alloc] init];
  self.mediator.consumer = self.viewController;
  self.viewController.modelDelegate = self.mediator;
  DCHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [self.mediator disconnect];
}

#pragma mark - LockdownModeViewControllerPresentationDelegate

- (void)lockdownModeViewControllerDidRemove:
    (LockdownModeViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate lockdownModeCoordinatorDidRemove:self];
}

@end
