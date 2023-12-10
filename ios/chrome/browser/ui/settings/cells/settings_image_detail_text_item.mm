// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

@implementation SettingsImageDetailTextItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsImageDetailTextCell class];
    _imageViewAlpha = 1.0f;
  }
  return self;
}

- (void)configureCell:(SettingsImageDetailTextCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.image = self.image;
  [cell setImageViewAlpha:self.imageViewAlpha];

  if (self.attributedText) {
    cell.textLabel.attributedText = self.attributedText;
  } else if (self.textColor) {
    cell.textLabel.textColor = self.textColor;
  } else {
    cell.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }

  if (self.detailTextColor) {
    cell.detailTextLabel.textColor = self.detailTextColor;
  } else {
    cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }

  if (self.imageViewTintColor) {
    [cell setImageViewTintColor:self.imageViewTintColor];
  }

  if (self.image && self.alignImageWithFirstLineOfText) {
    [cell alignImageWithFirstLineOfText:YES];
  }
}

@end
