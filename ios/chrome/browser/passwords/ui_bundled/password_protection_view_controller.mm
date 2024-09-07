// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/password_protection_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_constants.h"
#import "ui/base/l10n/l10n_util.h"

@implementation PasswordProtectionViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.image = [UIImage imageNamed:@"legacy_password_breach_illustration"];
  self.titleString = l10n_util::GetNSString(
      IDS_PAGE_INFO_CHANGE_PASSWORD_SAVED_PASSWORD_SUMMARY);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_PAGE_INFO_CHECK_PASSWORDS_BUTTON);
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kPasswordProtectionViewAccessibilityIdentifier;
}

@end
