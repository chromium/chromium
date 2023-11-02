// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_mediator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/histograms.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"
#import "ios/public/provider/chrome/browser/password_auto_fill/password_auto_fill_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  RecordEventOnUMA(PasswordsInOtherAppsActionAutoFillStatusChange);
  [self.consumer updateInstructionsWithCurrentPasswordAutoFillStatus];
}

#pragma mark - PasswordsInOtherAppsViewControllerDelegate

- (void)openApplicationSettings {
  RecordEventOnUMA(PasswordsInOtherAppsActionGoToIOSSetting);
  ios::provider::PasswordsInOtherAppsOpensSettings();
}

@end
