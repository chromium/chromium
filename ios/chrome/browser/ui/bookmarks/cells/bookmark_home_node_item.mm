// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_home_node_item.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_folder_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarkHomeNodeItem
@synthesize bookmarkNode = _bookmarkNode;

- (instancetype)initWithType:(NSInteger)type
                bookmarkNode:(const bookmarks::BookmarkNode*)node {
  if ((self = [super initWithType:type])) {
    if (node->is_folder()) {
      self.cellClass = [TableViewBookmarkFolderCell class];
    } else {
      self.cellClass = [TableViewURLCell class];
    }
    _bookmarkNode = node;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  if (_bookmarkNode->is_folder()) {
    TableViewBookmarkFolderCell* bookmarkCell =
        base::mac::ObjCCastStrict<TableViewBookmarkFolderCell>(cell);
    bookmarkCell.folderTitleTextField.text =
        bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
    bookmarkCell.folderImageView.image =
        [UIImage imageNamed:@"bookmark_blue_folder"];
    bookmarkCell.bookmarkAccessoryType =
        TableViewBookmarkFolderAccessoryTypeDisclosureIndicator;
    bookmarkCell.accessibilityIdentifier =
        bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
    bookmarkCell.accessibilityTraits |= UIAccessibilityTraitButton;
  } else {
    TableViewURLCell* urlCell =
        base::mac::ObjCCastStrict<TableViewURLCell>(cell);
    urlCell.titleLabel.text =
        bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
    urlCell.URLLabel.text = base::SysUTF16ToNSString(
        url_formatter::
            FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                _bookmarkNode->url()));
    urlCell.accessibilityTraits |= UIAccessibilityTraitButton;
    [urlCell configureUILayout];
  }
}

@end
