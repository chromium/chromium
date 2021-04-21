// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_type.h"
#import "ios/chrome/browser/ui/first_run/signin_screen_coordinator.h"
#import "ios/chrome/browser/ui/first_run/welcome_screen_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FirstRunCoordinator () <FirstRunScreenDelegate>

@property(nonatomic, strong) FirstRunScreenProvider* screenProvider;
@property(nonatomic, strong) ChromeCoordinator* childCoordinator;
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation FirstRunCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            screenProvider:
                                (FirstRunScreenProvider*)screenProvider {
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
  [self.baseViewController presentViewController:self.navigationController
                                        animated:NO
                                      completion:nil];
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
  [self.delegate willFinishPresentingScreens];
}

#pragma mark - Helper

// Presents the screen of certain |type|.
- (void)presentScreen:(NSNumber*)type {
  // If no more screen need to be present, call delegate to stop presenting
  // screens.
  if ([type isEqualToNumber:@(kFirstRunCompleted)])
    [self.delegate willFinishPresentingScreens];
  self.childCoordinator = [self createChildCoordinatorWithScreenType:type];
  [self.childCoordinator start];
}

// Creates a screen coordinator according to |type|.
- (ChromeCoordinator*)createChildCoordinatorWithScreenType:(NSNumber*)type {
  switch ([type integerValue]) {
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
    case kDefaultBrowserPromo:
      // TODO (crbug.com/1189807): Create screen coordinators for sign-in, sync
      // an default browser screen.
      return nil;
  }
  return nil;
}

@end
