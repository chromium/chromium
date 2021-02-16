// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_protection_view_controller.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/passwords/password_constants.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordProtectionViewController

#pragma mark - Public

- (void)viewDidLoad {
  self.image = [UIImage imageNamed:@"password_breach_illustration"];
  self.helpButtonAvailable = NO;
  self.primaryActionAvailable = YES;
  self.titleString =
      l10n_util::GetNSString(IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_PAGE_INFO_CHECK_PASSWORDS_BUTTON);
  if (@available(iOS 13.4, *)) {
    self.pointerInteractionEnabled = YES;
  }
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kPasswordProtectionViewAccessibilityIdentifier;
}

@end
