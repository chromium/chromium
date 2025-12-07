// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#pragma mark - TableViewDetailTextItem

@implementation TableViewDetailTextItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = self.text;
  configuration.titleColor = self.textColor;
  configuration.titleNumberOfLines = 1;
  configuration.subtitle = self.detailText;
  configuration.subtitleColor = self.detailTextColor;
  configuration.subtitleNumberOfLines = self.allowMultilineDetailText ? 0 : 1;

  cell.contentConfiguration = configuration;

  NSString* accessibilityLabel = configuration.accessibilityLabel;
  // If the cell indicates an external link, append an accessibility hint for
  // screen readers.
  if (self.accessorySymbol ==
      TableViewDetailTextCellAccessorySymbolExternalLink) {
    NSString* hint = l10n_util::GetNSString(IDS_IOS_OPENS_IN_NEW_TAB);
    accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", accessibilityLabel, hint];
  }

  cell.accessibilityLabel = accessibilityLabel;
  cell.accessibilityValue = configuration.accessibilityValue;
  cell.accessibilityHint = configuration.accessibilityHint;

  // Accessory symbol.
  switch (self.accessorySymbol) {
    case TableViewDetailTextCellAccessorySymbolChevron:
      cell.accessoryView = [[UIImageView alloc]
          initWithImage:DefaultSymbolTemplateWithPointSize(
                            kChevronForwardSymbol, kSymbolAccessoryPointSize)];
      break;
    case TableViewDetailTextCellAccessorySymbolExternalLink:
      cell.accessoryView = [[UIImageView alloc]
          initWithImage:DefaultSymbolTemplateWithPointSize(
                            kExternalLinkSymbol, kSymbolAccessoryPointSize)];
      break;
    case TableViewDetailTextCellAccessorySymbolNone:
      cell.accessoryView = nil;
      break;
  }
  if (cell.accessoryView) {
    // Hard code color until other use cases arise.
    cell.accessoryView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  }
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
}

@end
