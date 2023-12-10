// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "url/gurl.h"

#pragma mark - SnippetSearchEngineItem

@implementation SnippetSearchEngineItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = SnippetSearchEngineCell.class;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  SnippetSearchEngineCell* cell =
      base::apple::ObjCCastStrict<SnippetSearchEngineCell>(tableCell);
  cell.nameLabel.text = self.name;
  cell.snippetLabel.text = self.snippetDescription;
  cell.snippetState = self.snippetState;
  cell.checked = self.checked;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;
  cell.userInteractionEnabled = YES;
  cell.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  cell.faviconImage = self.faviconImage;
  if (styler.cellTitleColor) {
    cell.nameLabel.textColor = styler.cellTitleColor;
  }
}

- (BOOL)isEqual:(SnippetSearchEngineItem*)otherItem {
  return (self.name == otherItem.name) && (self.URL == otherItem.URL);
}

@end
