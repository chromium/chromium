// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#pragma mark - TableViewTextItem

@implementation TableViewTextItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
    _enabled = YES;
    _titleNumberOfLines = 1;
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.isAccessibilityElement = YES;

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  if (self.masked) {
    configuration.title = kMaskedPassword;
    cell.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL);
  } else {
    if (self.useHeadlineFont) {
      configuration.attributedTitle = [[NSAttributedString alloc]
          initWithString:self.text
              attributes:@{
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
              }];
    } else {
      configuration.title = self.text;
    }
    cell.accessibilityLabel =
        self.accessibilityLabel ? self.accessibilityLabel : self.text;
  }
  configuration.titleColor = self.textColor;
  configuration.titleNumberOfLines = self.titleNumberOfLines;

  cell.contentConfiguration = configuration;

  cell.userInteractionEnabled = self.enabled;
  if (self.checked) {
    cell.accessoryView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"bookmark_blue_check"]];
  }
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
}

@end
