// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordSettingsCoordinator () <PasswordSettingsPresentationDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong)
    PasswordSettingsViewController* passwordSettingsViewController;

@end

@implementation PasswordSettingsCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.passwordSettingsViewController =
      [[PasswordSettingsViewController alloc] init];

  self.passwordSettingsViewController.presentationDelegate = self;

  [self.baseViewController
      presentViewController:self.passwordSettingsViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  self.passwordSettingsViewController = nil;
}

#pragma mark - PasswordSettingsPresentationDelegate

- (void)passwordSettingsViewControllerDidDismiss {
  [self.delegate passwordSettingsCoordinatorDidRemove:self];
}

@end
