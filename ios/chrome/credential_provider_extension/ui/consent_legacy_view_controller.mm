// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/consent_legacy_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr CGFloat kStackViewSpacingAfterIllustration = 37;
}  // namespace

@implementation ConsentLegacyViewController

#pragma mark - Public

- (void)loadView {
  self.image = [UIImage imageNamed:@"consent_illustration"];
  self.customSpacingAfterImage = kStackViewSpacingAfterIllustration;

  self.helpButtonAvailable = YES;
  self.helpButtonAccessibilityLabel = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_HELP_ACCESSIBILITY_LABEL", @"Help.");

  NSString* titleString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_TITLE_LEGACY",
                        @"The title in the consent screen.");
  NSString* subtitleString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_SUBTITLE_LEGACY",
                        @"The subtitle in the consent screen.");
  NSString* primaryActionString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_ENABLE_BUTTON_TITLE_LEGACY",
      @"The primary action title in the consent screen. Used to explicitly "
      @"enable the extension.");
  self.titleString = titleString;
  self.subtitleString = subtitleString;
  self.primaryActionString = primaryActionString;
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemCancel;
  [super loadView];
}

@end
