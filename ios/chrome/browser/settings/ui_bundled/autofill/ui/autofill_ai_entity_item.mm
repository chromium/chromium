// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/ui/autofill_ai_entity_item.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation AutofillAiEntityItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  TableViewCellContentConfiguration* contentConfiguration =
      [[TableViewCellContentConfiguration alloc] init];

  contentConfiguration.title = self.name;
  contentConfiguration.subtitle = self.typeDescription;
  contentConfiguration.trailingText = self.trailingText;

  // Configure icon.
  if (self.icon) {
    ColorfulSymbolContentConfiguration* symbolConfiguration =
        [[ColorfulSymbolContentConfiguration alloc] init];
    symbolConfiguration.symbolImage = self.icon;
    // Use default colors or customize if needed.
    symbolConfiguration.symbolTintColor =
        [UIColor colorNamed:kTextPrimaryColor];
    contentConfiguration.leadingConfiguration = symbolConfiguration;
  }

  cell.contentConfiguration = contentConfiguration;
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
}

@end
