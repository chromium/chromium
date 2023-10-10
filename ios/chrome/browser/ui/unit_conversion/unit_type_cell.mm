// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/unit_conversion/unit_type_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Leading constraint of the chosen unit label.
const CGFloat kUnitTypeLabelLeadingOffset = 20;

// Size and height/trailing constraint of the unit chooser button.
const CGFloat kUnitMenuButtonIconSize = 16;
const CGFloat kUnitMenuButtonHeightOffset = 10;
const CGFloat kUnitMenuButtonTrailingOffset = 16;

// Cells height anchors constraint.
const CGFloat kUnitTypeCellHeightAnchor = 40;

// ContentView's height anchor offset to add some space is case the font become
// too big.
const CGFloat kContentViewHeightAnchorOffset = 5;

}  // namespace

@implementation UnitTypeCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    // Set the unit type label.
    _unitTypeLabel = [[UILabel alloc] initWithFrame:self.frame];
    _unitTypeLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _unitTypeLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    _unitTypeLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_unitTypeLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_unitTypeLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kUnitTypeLabelLeadingOffset],
      [_unitTypeLabel.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];

    // Set the unit menu button.
    _unitMenuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    UIImage* chevronUpDown =
        DefaultSymbolWithPointSize(kChevronUpDown, kUnitMenuButtonIconSize);
    [_unitMenuButton setImage:chevronUpDown forState:UIControlStateNormal];
    [_unitMenuButton setTintColor:[UIColor colorNamed:kTextQuaternaryColor]];

    _unitMenuButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_unitMenuButton];

    [NSLayoutConstraint activateConstraints:@[
      [_unitMenuButton.heightAnchor
          constraintEqualToAnchor:self.contentView.heightAnchor
                         constant:-kUnitMenuButtonHeightOffset],
      [_unitMenuButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kUnitMenuButtonTrailingOffset],
      [_unitMenuButton.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_unitMenuButton.widthAnchor
          constraintEqualToConstant:kUnitMenuButtonIconSize],
      [_unitTypeLabel.trailingAnchor
          constraintEqualToAnchor:_unitMenuButton.leadingAnchor],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToAnchor:_unitTypeLabel.heightAnchor
                                      constant:kContentViewHeightAnchorOffset],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToConstant:kUnitTypeCellHeightAnchor],
    ]];
  }
  return self;
}

@end
