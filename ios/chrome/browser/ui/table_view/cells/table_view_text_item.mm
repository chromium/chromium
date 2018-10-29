// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Vertical spacing between label and the container view of a cell.
const CGFloat kLabelCellVerticalSpacing = 11.0;
}  // namespace

#pragma mark - TableViewTextItem

@implementation TableViewTextItem
@synthesize text = _text;
@synthesize textAlignment = _textAlignment;
@synthesize textColor = _textColor;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewTextCell class];
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewTextCell* cell =
      base::mac::ObjCCastStrict<TableViewTextCell>(tableCell);
  cell.textLabel.text = self.text;
  cell.textLabel.backgroundColor = styler.tableViewBackgroundColor;
  // This item's text color takes precedence over the global styler.
  // TODO(crbug.com/854249): redo the logic for this convoluted if clause.
  if (self.textColor) {
    cell.textLabel.textColor = self.textColor;
  } else if (styler.cellTitleColor) {
    cell.textLabel.textColor = styler.cellTitleColor;
  } else {
    cell.textLabel.textColor =
        UIColorFromRGB(kTableViewTextLabelColorLightGrey);
  }
  cell.textLabel.textAlignment =
      self.textAlignment ? self.textAlignment : NSTextAlignmentLeft;
}

@end

#pragma mark - TableViewTextCell

@implementation TableViewTextCell
@synthesize textLabel = _textLabel;
@synthesize checked = _checked;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    // Text Label, set font sizes using dynamic type.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];

    // Add subviews to View Hierarchy.
    [self.contentView addSubview:_textLabel];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      // Title Label Constraints.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                           constant:kLabelCellVerticalSpacing],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kLabelCellVerticalSpacing],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing]
    ]];
  }
  return self;
}

- (void)setChecked:(BOOL)checked {
  if (checked) {
    self.accessoryView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"bookmark_blue_check"]];
  } else {
    self.accessoryView = nil;
  }
  _checked = checked;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.checked = NO;
}

@end
