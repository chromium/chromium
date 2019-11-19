// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_add_account_item.h"

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_cell.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation IdentityChooserAddAccountItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [IdentityChooserCell class];
  }
  return self;
}

- (void)configureCell:(IdentityChooserCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT);
  UIImage* image = [[UIImage imageNamed:@"settings_accounts_add_account"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [cell configureCellWithTitle:title subtitle:nil image:image checked:NO];
}

@end
