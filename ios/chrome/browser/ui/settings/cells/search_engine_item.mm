// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/search_engine_item.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - SearchEngineItem

@interface SearchEngineItem ()

// Redefined as read write.
@property(nonatomic, readwrite, copy) NSString* uniqueIdentifier;

@end

@implementation SearchEngineItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
      self.cellClass = TableViewURLCell.class;
      _enabled = YES;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  self.uniqueIdentifier = base::SysUTF8ToNSString(self.URL.host());

  TableViewURLCell* cell =
      base::mac::ObjCCastStrict<TableViewURLCell>(tableCell);
  cell.titleLabel.text = self.text;
  cell.URLLabel.text = self.detailText;
  cell.cellUniqueIdentifier = self.uniqueIdentifier;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;
  if (self.enabled) {
    cell.contentView.alpha = 1.0;
    cell.userInteractionEnabled = YES;
    cell.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    cell.contentView.alpha = 0.4;
    cell.userInteractionEnabled = NO;
    cell.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }

  if (styler.cellTitleColor) {
    cell.titleLabel.textColor = styler.cellTitleColor;
  }

  cell.URLLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

  [cell configureUILayout];
}

@end
