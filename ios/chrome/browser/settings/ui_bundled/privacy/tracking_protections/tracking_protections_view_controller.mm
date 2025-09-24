// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/tracking_protections/tracking_protections_view_controller.h"

@implementation TrackingProtectionsViewController

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate trackingProtectionsViewControllerDidRemove:self];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/442799337): Record dismissal metric.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/442799337): Record back metric.
}

@end
