// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation AutofillAIEntityCountryItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  if ([cell.contentConfiguration
          isKindOfClass:[TableViewCellContentConfiguration class]]) {
    TableViewCellContentConfiguration* contentConfiguration =
        (TableViewCellContentConfiguration*)cell.contentConfiguration;
    contentConfiguration.trailingTextColor =
        [UIColor colorNamed:kTextPrimaryColor];
    cell.contentConfiguration = contentConfiguration;
  }
}

@end
