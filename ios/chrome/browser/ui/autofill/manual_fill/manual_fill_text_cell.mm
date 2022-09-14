// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_text_cell.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ManualFillTextItem

- (instancetype)initWithType:(NSInteger)type {
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
