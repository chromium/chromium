// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_summary_item.h"

#import "base/check.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {

// Dimensions of the credit card icon.
constexpr CGFloat kCardIconWidth = 40;
constexpr CGFloat kCardIconHeight = 24;

}  // namespace

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
  configuration.leadingConfiguration = [self createLeadingConfiguration];

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

#pragma mark - Private

// Creates the leading `ChromeContentConfiguration` matching `iconStyle`.
- (NSObject<ChromeContentConfiguration>*)createLeadingConfiguration {
  switch (self.iconStyle) {
    case ManualFillIconStyle::kSymbol: {
      ColorfulSymbolContentConfiguration* symbolConfiguration =
          [[ColorfulSymbolContentConfiguration alloc] init];
      symbolConfiguration.symbolImage = self.iconImage;
      symbolConfiguration.symbolTintColor =
          [UIColor colorNamed:kTextPrimaryColor];
      // Contrast the icon container against the cell's
      // `kGroupedSecondaryBackgroundColor` background.
      symbolConfiguration.symbolBackgroundColor =
          [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
      return symbolConfiguration;
    }
    case ManualFillIconStyle::kCardIcon: {
      ImageContentConfiguration* imageConfiguration =
          [[ImageContentConfiguration alloc] init];
      imageConfiguration.image = self.iconImage;
      imageConfiguration.imageSize =
          CGSizeMake(kCardIconWidth, kCardIconHeight);
      imageConfiguration.imageContentMode = UIViewContentModeScaleAspectFit;
      return imageConfiguration;
    }
    case ManualFillIconStyle::kFavicon: {
      // Credential entries without a site favicon (such as passkeys or backup
      // passwords) supply a symbol override.
      if (self.iconImage) {
        ImageContentConfiguration* imageConfiguration =
            [[ImageContentConfiguration alloc] init];
        imageConfiguration.image = self.iconImage;
        imageConfiguration.imageSize =
            CGSizeMake(kTableViewIconImageSize, kTableViewIconImageSize);
        imageConfiguration.imageContentMode = UIViewContentModeCenter;
        imageConfiguration.imageTintColor =
            [UIColor colorNamed:kTextPrimaryColor];
        return imageConfiguration;
      }
      FaviconContentConfiguration* faviconConfiguration =
          [[FaviconContentConfiguration alloc] init];
      faviconConfiguration.faviconAttributes = self.faviconAttributes;
      return faviconConfiguration;
    }
  }
}

@end
