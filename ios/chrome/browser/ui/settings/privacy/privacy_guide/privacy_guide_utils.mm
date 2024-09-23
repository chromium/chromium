// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_utils.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/self_sizing_table_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const CGFloat kSymbolSize = 20;
const CGFloat kSwitchCellCornerRadius = 12;

}  // namespace

SettingsImageDetailTextCell* PrivacyGuideExplanationCell(
    UITableView* table_view,
    int text_id,
    NSString* symbol_name) {
  // TODO(crbug.com/41492491): Remove the default insets in the
  // SettingsImageDetailTextCell.
  SettingsImageDetailTextCell* cell =
      DequeueTableViewCell<SettingsImageDetailTextCell>(table_view);

  cell.image = DefaultSymbolWithPointSize(symbol_name, kSymbolSize);
  cell.detailTextLabel.text = l10n_util::GetNSString(text_id);
  cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [cell setImageViewTintColor:[UIColor colorNamed:kTextSecondaryColor]];
  [cell alignImageWithFirstLineOfText:YES];
  [cell setUseCustomSeparator:NO];

  return cell;
}

TableViewSwitchCell* PrivacyGuideSwitchCell(UITableView* table_view,
                                            int text_id,
                                            BOOL switch_enabled,
                                            BOOL switch_on,
                                            NSString* accessibility_id) {
  TableViewSwitchCell* cell =
      DequeueTableViewCell<TableViewSwitchCell>(table_view);

  NSString* title = l10n_util::GetNSString(text_id);
  [cell configureCellWithTitle:title
                      subtitle:nil
                 switchEnabled:switch_enabled
                            on:switch_on];
  [cell setUseCustomSeparator:NO];
  cell.textLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  cell.textLabel.numberOfLines = 0;
  cell.textLabel.adjustsFontForContentSizeCategory = YES;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  cell.layer.cornerRadius = kSwitchCellCornerRadius;
  cell.accessibilityIdentifier = accessibility_id;

  return cell;
}

TableViewTextHeaderFooterView* PrivacyGuideHeaderView(UITableView* table_view,
                                                      int text_id) {
  TableViewTextHeaderFooterView* header =
      DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(table_view);
  header.textLabel.text = l10n_util::GetNSString(text_id);
  [header setSubtitle:nil];
  return header;
}

SelfSizingTableView* PrivacyGuideTableView() {
  SelfSizingTableView* tableView =
      [[SelfSizingTableView alloc] initWithFrame:CGRectZero
                                           style:ChromeTableViewStyle()];

  tableView.translatesAutoresizingMaskIntoConstraints = NO;
  tableView.alwaysBounceVertical = NO;
  tableView.backgroundColor = [UIColor clearColor];
  tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  tableView.separatorInset = UIEdgeInsetsZero;
  [tableView setLayoutMargins:UIEdgeInsetsZero];
  tableView.sectionHeaderTopPadding = 0;

  return tableView;
}
