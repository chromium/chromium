// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_action.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation ManualFillAction

#pragma mark - Public

- (instancetype)initWithIdentifier:(NSString*)identifier
                             title:(NSString*)title
           accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  self = [super init];
  if (self) {
    CHECK(identifier.length > 0);
    CHECK(title.length > 0);
    _identifier = [identifier copy];
    _title = [title copy];
    _accessibilityIdentifier = accessibilityIdentifier.length > 0
                                   ? [accessibilityIdentifier copy]
                                   : nil;
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)cell {
  CHECK(cell);

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = self.title;
  configuration.titleColor = [UIColor colorNamed:kBlueColor];

  cell.contentConfiguration = configuration;
  cell.backgroundColor = [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  cell.accessoryType = UITableViewCellAccessoryNone;
  cell.accessibilityIdentifier = self.accessibilityIdentifier;
  cell.accessibilityTraits = UIAccessibilityTraitButton;
}

@end
