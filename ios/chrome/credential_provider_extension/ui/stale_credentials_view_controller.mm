// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/stale_credentials_view_controller.h"

#import "ios/chrome/credential_provider_extension/generated_localized_strings.h"

namespace {
constexpr CGFloat kStackViewSpacingAfterIllustration = 32;
}  // namespace

@implementation StaleCredentialsViewController

#pragma mark - UIViewController

- (void)loadView {
  self.image = [UIImage imageNamed:@"empty_credentials_illustration"];
  self.customSpacingAfterImage = kStackViewSpacingAfterIllustration;
  self.titleString = CredentialProviderStaleCredentialsTitleString();
  self.subtitleString = CredentialProviderStaleCredentialsSubtitleString();

  [super loadView];
}

@end
