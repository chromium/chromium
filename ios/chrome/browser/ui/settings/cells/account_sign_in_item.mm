// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "ui/base/l10n/l10n_util.h"

@implementation AccountSignInItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsImageDetailTextCell class];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(SettingsImageDetailTextCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.detailTextLabel.text = self.detailText;
  cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  cell.textLabel.text =
      l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_SIGNIN_WITH_UNO);
  cell.image = DefaultSymbolTemplateWithPointSize(
      kPersonCropCircleSymbol, kAccountProfilePhotoDimension);
  [cell setImageViewTintColor:[UIColor colorNamed:kBlue600Color]];
}

@end
