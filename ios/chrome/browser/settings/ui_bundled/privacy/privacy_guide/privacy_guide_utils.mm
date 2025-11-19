// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_guide/privacy_guide_utils.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/self_sizing_table_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const CGFloat kSymbolSize = 20;

}  // namespace

UITableViewCell* PrivacyGuideExplanationCell(UITableView* table_view,
                                             int text_id,
                                             NSString* symbol_name) {
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:table_view];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.subtitle = l10n_util::GetNSString(text_id);

  ImageContentConfiguration* image_configuration =
      [[ImageContentConfiguration alloc] init];
  image_configuration.image =
      DefaultSymbolWithPointSize(symbol_name, kSymbolSize);
  image_configuration.imageTintColor = [UIColor colorNamed:kTextSecondaryColor];

  configuration.leadingConfiguration = image_configuration;
  cell.contentConfiguration = configuration;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  return cell;
}

UITableViewCell* PrivacyGuideSwitchCell(UITableView* table_view,
                                        int text_id,
                                        BOOL switch_on,
                                        NSString* accessibility_id,
                                        id target,
                                        SEL selector) {
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:table_view];

  NSString* title = l10n_util::GetNSString(text_id);
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = title;

  SwitchContentConfiguration* switch_configuration =
      [[SwitchContentConfiguration alloc] init];
  switch_configuration.on = switch_on;
  switch_configuration.target = target;
  switch_configuration.selector = selector;

  configuration.trailingConfiguration = switch_configuration;
  cell.contentConfiguration = configuration;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

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
