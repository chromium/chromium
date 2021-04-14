// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin_screen_coordinator.h"

#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/first_run/signin_screen_consumer.h"
#import "ios/chrome/browser/ui/first_run/signin_screen_mediator.h"
#import "ios/chrome/browser/ui/first_run/signin_screen_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninScreenCoordinator () <SigninScreenViewControllerDelegate>

// Sign-in screen view controller.
@property(nonatomic, strong) SigninScreenViewController* viewController;
// Sign-in screen mediator.
@property(nonatomic, strong) SigninScreenMediator* mediator;

@end

@implementation SigninScreenCoordinator

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
  // TODO(crbug.com/1189836): Check if sign-in screen need to be shown.
  // if not:
  // [self.delegate willFinishPresenting]
  // if yes:
  self.viewController = [[SigninScreenViewController alloc] init];
  self.viewController.delegate = self;
  self.mediator = [[SigninScreenMediator alloc] init];
  self.mediator.consumer = self.viewController;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  self.delegate = nil;
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - SigninScreenViewControllerDelegate

- (void)showAccountPicker {
  // Create a IdentityChooserCoordinator and shows the account picker UI.
}

@end
