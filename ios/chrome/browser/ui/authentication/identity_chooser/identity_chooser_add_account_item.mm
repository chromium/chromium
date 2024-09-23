// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_add_account_item.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation IdentityChooserAddAccountItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewIdentityCell class];
    self.useCustomSeparator = NO;
  }
  return self;
}

- (void)configureCell:(TableViewIdentityCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT);
  UIImage* image = [[UIImage imageNamed:@"settings_accounts_add_account"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  cell.accessibilityIdentifier = kIdentityPickerAddAccountIdentifier;
  [cell configureCellWithTitle:title
                      subtitle:nil
                         image:image
                       checked:NO
             identityViewStyle:IdentityViewStyleIdentityChooser
                    titleColor:[UIColor colorNamed:kBlueColor]];
}

@end
