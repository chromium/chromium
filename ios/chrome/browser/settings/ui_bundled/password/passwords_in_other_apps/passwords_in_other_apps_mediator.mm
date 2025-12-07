// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/passwords_in_other_apps/passwords_in_other_apps_mediator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/settings/ui_bundled/utils/password_auto_fill_status_manager.h"
#import "ios/chrome/browser/shared/coordinator/utils/credential_provider_settings_utils.h"

@implementation PasswordsInOtherAppsMediator

- (instancetype)init {
  self = [super init];
  if (self) {
    [[PasswordAutoFillStatusManager sharedManager] addObserver:self];
  }
  return self;
}

- (void)dealloc {
  [[PasswordAutoFillStatusManager sharedManager] removeObserver:self];
}

#pragma mark - PasswordAutofillStatusObserver

- (void)passwordAutoFillStatusDidChange {
  // Since this action is appended to the main queue, at this stage,
  // self.consumer should have already been setup.
  DCHECK(self.consumer);
  [self.consumer updateInstructionsWithCurrentPasswordAutoFillStatus];
}

#pragma mark - PasswordsInOtherAppsViewControllerDelegate

- (void)openApplicationSettings {
  OpenIOSCredentialProviderSettings();
}

@end
