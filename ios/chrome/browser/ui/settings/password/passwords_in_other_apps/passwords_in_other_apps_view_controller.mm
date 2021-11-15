// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_view_controller.h"

#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordsInOtherAppsViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  // TODO(crbug.com/1252110): draw the UI of self.view.
  if ([PasswordAutoFillStatusManager sharedManager].ready) {
    [self updateInstructionsWithCurrentPasswordAutoFillStatus];
  } else {
    // TODO(crbug.com/1252110): draw self.specificContentView in case password
    // auto-fill status is unknown.
  }
  [super viewDidLoad];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presenter passwordsInOtherAppsViewControllerDidDismiss];
  }
}

#pragma mark - PasswordsInOtherAppsConsumer

- (void)updateInstructionsWithCurrentPasswordAutoFillStatus {
  [PasswordAutoFillStatusManager sharedManager].autoFillEnabled
      ? [self showInstructionsToTurnOffAutoFill]
      : [self showInstructionsToTurnOnAutoFill];
}

#pragma mark - Private

- (void)showInstructionsToTurnOnAutoFill {
  // TODO(crbug.com/1252110): show instructions on how to turn on auto-fill.
}

- (void)showInstructionsToTurnOffAutoFill {
  // TODO(crbug.com/1252110): show instructions on how to turn off auto-fill.
}

@end
