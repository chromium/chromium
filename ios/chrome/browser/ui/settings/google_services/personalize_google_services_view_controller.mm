// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_view_controller.h"

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation PersonalizeGoogleServicesViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_IOS_MANAGE_SYNC_PERSONALIZE_GOOGLE_SERVICES_TITLE);
  self.view.accessibilityIdentifier = kPersonalizeGoogleServicesViewIdentifier;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        personalizeGoogleServicesViewcontrollerDidRemove:self];
  }
}

@end
