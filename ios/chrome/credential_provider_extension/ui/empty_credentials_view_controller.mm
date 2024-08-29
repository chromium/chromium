// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/empty_credentials_view_controller.h"

namespace {
constexpr CGFloat kStackViewSpacingAfterIllustration = 32;
}  // namespace

@implementation EmptyCredentialsViewController

#pragma mark - UIViewController

- (void)loadView {
  self.image = [UIImage imageNamed:@"empty_credentials_illustration"];
  self.customSpacingAfterImage = kStackViewSpacingAfterIllustration;
  self.titleString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_EMPTY_CREDENTIALS_TITLE",
                        @"The title in the empty credentials screen.");
  self.subtitleString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_EMPTY_CREDENTIALS_SUBTITLE",
      @"The subtitle in the empty credentials screen.");

  [super loadView];
}

@end
