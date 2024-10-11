// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller.h"

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller_presentation_delegate.h"

@implementation IncognitoLockViewController

#pragma mark - UIViewController

- (void)viewDidDisappear:(BOOL)animated {
  [self.presentationDelegate incognitoLockViewControllerDidRemove:self];
  [super viewDidDisappear:animated];
}

#pragma mark - SettingsControllerProtocol

// Called when user dismissed settings. View controllers must implement this
// method and report dismissal User Action.
- (void)reportDismissalUserAction {
  // TODO(crbug.com/370804664): Report dismissal metric of the incognito setting
  // page.
}

// Called when user goes back from a settings view controller. View controllers
// must implement this method and report appropriate User Action.
- (void)reportBackUserAction {
  // TODO(crbug.com/370804664): Report back button metric from the incognito
  // setting page.
}

@end
