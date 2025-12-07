// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The max number of lines for the cell title label.
const int kMaxNumberOfLinesForCellTitleLabel = 2;

}  // namespace

#pragma mark - TableViewURLItem

@implementation TableViewURLItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
    _titleLineBreakMode = NSLineBreakByTruncatingTail;
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];

  configuration.title = [self titleLabelText];
  configuration.titleNumberOfLines = kMaxNumberOfLinesForCellTitleLabel;
  configuration.titleLineBreakMode = self.titleLineBreakMode;
  configuration.subtitle = [self URLLabelText];
  configuration.secondSubtitle = self.thirdRowText;

  FaviconContentConfiguration* faviconConfiguration =
      [[FaviconContentConfiguration alloc] init];
  faviconConfiguration.faviconAttributes = self.faviconAttributes;

  configuration.leadingConfiguration = faviconConfiguration;

  cell.contentConfiguration = configuration;

  cell.accessibilityIdentifier = configuration.title;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;
  cell.accessibilityLabel = configuration.accessibilityLabel;
  cell.accessibilityValue = configuration.accessibilityValue;
  cell.accessibilityUserInputLabels =
      configuration.accessibilityUserInputLabels;
}

- (NSString*)uniqueIdentifier {
  if (!self.URL) {
    return @"";
  }
  return base::SysUTF8ToNSString(self.URL.gurl.GetHost());
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
}

#pragma mark Private

// Returns the text to use when configuring a title.
- (NSString*)titleLabelText {
  if (self.title.length) {
    return self.title;
  }
  if (!self.URL) {
    return @"";
  }
  NSString* hostname = [self displayedURL];
  if (hostname.length) {
    return hostname;
  }
  // Backup in case host returns nothing (e.g. about:blank).
  return base::SysUTF8ToNSString(self.URL.gurl.spec());
}

// Returns the text to use when configuring a URL.
- (NSString*)URLLabelText {
  // Use detail text instead of the URL if there is one set.
  if (self.detailText) {
    return self.detailText;
  }

  if (!self.title.length) {
    return nil;
  }

  // Append the hostname.
  if (!self.URL) {
    return @"";
  }

  NSString* hostname = [self displayedURL];
  return hostname;
}

- (NSString*)displayedURL {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              self.URL.gurl));
}

@end
