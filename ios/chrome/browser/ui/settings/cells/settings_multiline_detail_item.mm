// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_multiline_detail_item.h"

#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SettingsMultilineDetailItem

@synthesize text = _text;
@synthesize detailText = _detailText;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsMultilineDetailCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(SettingsMultilineDetailCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
}

@end

@implementation SettingsMultilineDetailCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = UIColor.cr_labelColor;
    [contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.numberOfLines = 0;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
    [contentView addSubview:_detailTextLabel];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.topAnchor
          constraintEqualToAnchor:contentView.topAnchor
                         constant:kTableViewLargeVerticalSpacing],
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:contentView.trailingAnchor
                                   constant:-kTableViewHorizontalSpacing],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor],
      [_detailTextLabel.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
      [_detailTextLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:contentView.trailingAnchor
                                   constant:-kTableViewHorizontalSpacing],
      [_detailTextLabel.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor
                         constant:-kTableViewLargeVerticalSpacing],
    ]];
  }
  return self;
}

- (void)layoutSubviews {
  // Make sure that the multiline labels' width isn't changed when the accessory
  // is set.
  self.detailTextLabel.preferredMaxLayoutWidth =
      self.bounds.size.width -
      (kTableViewAccessoryWidth + 2 * kTableViewHorizontalSpacing);
  self.textLabel.preferredMaxLayoutWidth =
      self.bounds.size.width -
      (kTableViewAccessoryWidth + 2 * kTableViewHorizontalSpacing);
  [super layoutSubviews];
}

#pragma mark Accessibility

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                    self.detailTextLabel.text];
}

@end
