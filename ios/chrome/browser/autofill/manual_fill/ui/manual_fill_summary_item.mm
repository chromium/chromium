// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_summary_item.h"

#import "base/check.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

@implementation ManualFillSummaryItem

#pragma mark - Public

- (instancetype)initWithIdentifier:(NSString*)identifier
                             title:(NSString*)title
                          subtitle:(NSString*)subtitle
                         iconStyle:(ManualFillIconStyle)iconStyle
                         iconImage:(UIImage*)iconImage {
  self = [super init];
  if (self) {
    CHECK(identifier.length > 0);
    CHECK(title.length > 0);
    _identifier = [identifier copy];
    _title = [title copy];
    _subtitle = subtitle.length > 0 ? [subtitle copy] : nil;
    _iconStyle = iconStyle;
    _iconImage = iconImage;
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)cell {
  CHECK(cell);

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = self.title;
  configuration.titleNumberOfLines = 1;
  configuration.titleLineBreakMode = NSLineBreakByTruncatingTail;
  configuration.subtitle = self.subtitle;
  configuration.subtitleNumberOfLines = 1;
  configuration.subtitleLineBreakMode = NSLineBreakByTruncatingTail;
  configuration.leadingConfiguration = CreateManualFillIconConfiguration(
      self.iconStyle, self.iconImage, self.faviconAttributes);

  if (self.cellIndexAccessibilityLabel.length > 0) {
    configuration.customAccessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", self.cellIndexAccessibilityLabel,
                                   configuration.accessibilityLabel];
  }

  cell.contentConfiguration = configuration;
  cell.backgroundColor = [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  cell.accessibilityIdentifier = manual_fill::kExpandedManualFillSummaryCellID;
  // Overwrite rather than bitwise-ORing because `TableViewCell` does not reset
  // `accessibilityTraits` in `-prepareForReuse`.
  cell.accessibilityTraits = UIAccessibilityTraitButton;
}

@end
