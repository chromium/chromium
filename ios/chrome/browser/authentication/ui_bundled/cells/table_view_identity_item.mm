// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_identity_item.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_identity_cell.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation TableViewIdentityItem

@synthesize gaiaID = _gaiaID;
@synthesize name = _name;
@synthesize email = _email;
@synthesize avatar = _avatar;
@synthesize selected = _selected;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewIdentityCell class];
    _identityViewStyle = IdentityViewStyleDefault;
    self.useCustomSeparator = YES;
  }
  return self;
}

- (void)configureCell:(TableViewIdentityCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  NSString* title = self.name;
  NSString* subtitle = self.email;
  if (!title.length) {
    title = subtitle;
    subtitle = nil;
  }
  cell.accessibilityIdentifier = self.email;
  // When not set, the screen readers will read this cell as "name, email".
  // Add a custom accessibility label for managed accounts to append "managed by
  // your organization" so that the screen readers read this cell as "name,
  // email, managed by your organization".
  if (AreSeparateProfilesForManagedAccountsEnabled() && self.managed) {
    cell.accessibilityLabel =
        self.name
            ? l10n_util::GetNSStringF(
                  IDS_IOS_SIGNIN_ACCOUNT_PICKER_CHOOSE_ACCOUNT_ITEM_DESCRIPTION_WITH_NAME_AND_EMAIL_MANAGED,
                  base::SysNSStringToUTF16(self.name),
                  base::SysNSStringToUTF16(self.email))
            : l10n_util::GetNSStringF(
                  IDS_IOS_SIGNIN_ACCOUNT_PICKER_CHOOSE_ACCOUNT_ITEM_DESCRIPTION_WITH_EMAIL_MANAGED,
                  base::SysNSStringToUTF16(self.email));
  }
  [cell configureCellWithTitle:title
                      subtitle:subtitle
                         image:self.avatar
                       checked:self.selected
                       managed:self.managed
             identityViewStyle:self.identityViewStyle
                    titleColor:[UIColor colorNamed:kTextPrimaryColor]];
}

@end
