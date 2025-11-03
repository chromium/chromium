// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_home_node_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/bookmarks/folder_chooser/ui/table_view_bookmarks_folder_item.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// Number of lines for the title.
const NSInteger kNumberOfTitleLines = 2;
}  // namespace

@implementation BookmarksHomeNodeItem
@synthesize bookmarkNode = _bookmarkNode;

- (instancetype)initWithType:(NSInteger)type
                bookmarkNode:(const bookmarks::BookmarkNode*)node {
  if ((self = [super initWithType:type])) {
    if (node->is_folder()) {
      self.cellClass = [TableViewBookmarksFolderCell class];
    } else {
      self.cellClass = LegacyTableViewCell.class;
    }
    _bookmarkNode = node;
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  if (_bookmarkNode->is_folder()) {
    TableViewBookmarksFolderCell* bookmarkCell =
        base::apple::ObjCCastStrict<TableViewBookmarksFolderCell>(cell);
    bookmarkCell.folderTitleTextField.text =
        bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
    bookmarkCell.folderImageView.image =
        [UIImage imageNamed:@"bookmark_blue_folder"];
    bookmarkCell.bookmarksAccessoryType =
        BookmarksFolderAccessoryTypeDisclosureIndicator;
    bookmarkCell.accessibilityIdentifier =
        bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
    bookmarkCell.accessibilityTraits |= UIAccessibilityTraitButton;
    bookmarkCell.cloudSlashedView.hidden = !self.shouldDisplayCloudSlashIcon;
  } else {
    TableViewCellContentConfiguration* configuration =
        [[TableViewCellContentConfiguration alloc] init];

    configuration.title =
        bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
    configuration.titleNumberOfLines = kNumberOfTitleLines;
    configuration.subtitle = base::SysUTF16ToNSString(
        url_formatter::
            FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                _bookmarkNode->url()));

    FaviconContentConfiguration* faviconConfiguration =
        [[FaviconContentConfiguration alloc] init];
    faviconConfiguration.faviconAttributes = self.faviconAttributes;

    configuration.leadingConfiguration = faviconConfiguration;

    if (self.shouldDisplayCloudSlashIcon) {
      ImageContentConfiguration* imageConfiguration =
          [[ImageContentConfiguration alloc] init];
      imageConfiguration.image =
          SymbolWithPalette(CustomSymbolWithPointSize(
                                kCloudSlashSymbol, kCloudSlashSymbolPointSize),
                            @[ CloudSlashTintColor() ]);

      configuration.trailingConfiguration = imageConfiguration;
    }

    cell.contentConfiguration = configuration;

    cell.accessibilityLabel = configuration.accessibilityLabel;
    cell.accessibilityValue = configuration.accessibilityValue;
    cell.accessibilityIdentifier = configuration.title;

    cell.accessibilityTraits |= UIAccessibilityTraitButton;
  }
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  if (_bookmarkNode->is_folder()) {
    return [super cellForTableView:tableView];
  } else {
    [TableViewCellContentConfiguration
        legacyRegisterCellForTableView:tableView];
    return [TableViewCellContentConfiguration
        legacyDequeueTableViewCell:tableView];
  }
}

@end
