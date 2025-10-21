// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

@implementation TableViewImageItem

@synthesize image = _image;
@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
    _enabled = YES;
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = self.title;
  configuration.subtitle = self.detailText;
  configuration.titleColor = self.textColor;
  configuration.subtitleColor = self.detailTextColor;

  if (self.image) {
    ColorfulSymbolContentConfiguration* symbolConfiguration =
        [[ColorfulSymbolContentConfiguration alloc] init];
    symbolConfiguration.symbolImage = self.image;
    configuration.leadingConfiguration = symbolConfiguration;
  }

  cell.contentConfiguration = configuration;
  cell.accessibilityLabel = configuration.accessibilityLabel;
  if (self.title) {
    cell.accessibilityUserInputLabels = @[ self.title ];
  }
  cell.accessibilityValue = configuration.accessibilityValue;
  cell.accessibilityHint = configuration.accessibilityHint;

  cell.userInteractionEnabled = self.enabled;
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
}

@end
