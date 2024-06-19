// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_text_cell.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"

@implementation ManualFillTextItem

- (instancetype)initWithType:(NSInteger)type {
  // TODO(crbug.com/326398845): Completely remove the ManualFillTextItem/Cell
  // classes once the Keyboard Accessory Upgrade feature has launched both on
  // iPhone and iPad.
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());

  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ManualFillTextCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillTextCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  if (self.showSeparator) {
    cell.showSeparator = self.showSeparator;
  }
}

@end

@interface ManualFillTextCell ()
// Separator line after cell, if needed.
@property(nonatomic, weak) UIView* grayLine;
@end

@implementation ManualFillTextCell

- (void)setShowSeparator:(BOOL)showSeparator {
  _showSeparator = showSeparator;
  if (!showSeparator) {
    [self.grayLine removeFromSuperview];
  } else if (!self.grayLine) {
    UIView* grayLine = CreateGraySeparatorForContainer(self.contentView);
    self.grayLine = grayLine;
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.showSeparator = NO;
}

@end
