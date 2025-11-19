// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

@implementation SettingsImageDetailTextItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
    _alpha = 1;
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  if (self.attributedText) {
    configuration.attributedTitle = self.attributedText;
  } else {
    configuration.title = self.text;
    configuration.titleColor = self.textColor;
  }
  configuration.subtitle = self.detailText;
  configuration.subtitleColor = self.detailTextColor;

  if (self.image) {
    ImageContentConfiguration* imageConfiguration =
        [[ImageContentConfiguration alloc] init];
    imageConfiguration.image = self.image;
    imageConfiguration.imageTintColor = self.imageViewTintColor;
    configuration.leadingConfiguration = imageConfiguration;
  }

  cell.contentConfiguration = configuration;
  cell.contentView.alpha = self.alpha;
  cell.accessibilityLabel = configuration.accessibilityLabel;
  cell.accessibilityUserInputLabels =
      configuration.accessibilityUserInputLabels;
  cell.accessibilityValue = configuration.accessibilityValue;
  cell.accessibilityHint = configuration.accessibilityHint;

  if (self.accessibilityElementsHidden) {
    cell.accessibilityElementsHidden = self.accessibilityElementsHidden;
  }
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
}

@end
