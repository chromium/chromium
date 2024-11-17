// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/saving_enterprise_disabled_view_controller.h"

namespace {

// Point size of the enterprise icon.
const CGFloat kIconPointSize = 60.0;

}  // namespace

@implementation SavingEnterpriseDisabledViewController

#pragma mark - UIViewController

- (void)loadView {
  self.image = [UIImage imageNamed:@"cpe_enterprise_icon"
                          inBundle:nil
                 withConfiguration:
                     [UIImageSymbolConfiguration
                         configurationWithPointSize:kIconPointSize
                                             weight:UIImageSymbolWeightMedium
                                              scale:UIImageSymbolScaleMedium]];
  self.imageHasFixedSize = YES;
  self.titleString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_CREATION_ENTERPRISE_DISABLED_TITLE",
      @"The title in the passkey creation enterprise disabled screen.");
  self.subtitleString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_CREATION_"
      @"ENTERPRISE_DISABLED_SUBTITLE",
      @"The subtitle in the passkey creation enterprise disabled screen.");
  [super loadView];
}

@end
