// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/saving_enterprise_disabled_view_controller.h"

@implementation SavingEnterpriseDisabledViewController

#pragma mark - UIViewController

- (void)loadView {
  // TODO(crbug.com/362719658): Add correct image and strings.
  self.image = [UIImage imageNamed:@"empty_credentials_illustration"];
  self.titleString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_STALE_CREDENTIALS_TITLE",
                        @"The title in the stale credentials screen.");
  self.subtitleString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_STALE_CREDENTIALS_SUBTITLE",
      @"The subtitle in the stale credentials screen.");
  [super loadView];
}

@end
