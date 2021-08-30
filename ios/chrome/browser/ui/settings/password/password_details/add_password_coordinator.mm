// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AddPasswordCoordinator () <
    AddPasswordHandler,
    UIAdaptivePresentationControllerDelegate> {
  // Manager responsible for getting existing password profiles.
  IOSChromePasswordCheckManager* _manager;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordDetailsTableViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) AddPasswordMediator* mediator;

// Module containing the reauthentication mechanism for editing existing
// passwords.
@property(nonatomic, weak) ReauthenticationModule* reauthenticationModule;

@end

@implementation AddPasswordCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              reauthModule:(ReauthenticationModule*)reauthModule
                      passwordCheckManager:
                          (IOSChromePasswordCheckManager*)manager {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    DCHECK(viewController);
    DCHECK(manager);
    _manager = manager;
    _reauthenticationModule = reauthModule;
  }
  return self;
}

- (void)start {
  self.viewController = [[PasswordDetailsTableViewController alloc]
      initWithIsAddingNewCredential:YES];

  self.mediator =
      [[AddPasswordMediator alloc] initWithPasswordCheckManager:_manager];
  self.viewController.addPasswordHandler = self;
  self.viewController.delegate = self.mediator;
  self.viewController.reauthModule = self.reauthenticationModule;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.navigationController dismissViewControllerAnimated:YES
                                                               completion:nil];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - AddPasswordHandler

- (void)dismissPasswordDetailsTableViewController {
  [self.delegate passwordDetailsTableViewControllerDidFinish:self];
}

@end
