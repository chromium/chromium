// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_type_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Size and height/trailing/leading constraints of the unit chooser button.
const CGFloat kUnitMenuButtonLeadingOffset = 20;
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
    UIButtonConfiguration* unitMenuButtonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    unitMenuButtonConfiguration.imagePlacement = NSDirectionalRectEdgeTrailing;
    unitMenuButtonConfiguration.contentInsets =
        NSDirectionalEdgeInsetsMake(0, 0, 0, 0);

    UIImage* chevronUpDownDefault =
        DefaultSymbolWithPointSize(kChevronUpDown, kUnitMenuButtonIconSize);
    UIColor* chevronUpDownColor = [UIColor colorNamed:kTextTertiaryColor];
    UIImage* chevronUpDown = [chevronUpDownDefault
        imageByApplyingSymbolConfiguration:
            [UIImageSymbolConfiguration
                configurationWithHierarchicalColor:chevronUpDownColor]];

    _unitMenuButton =
        [UIButton buttonWithConfiguration:unitMenuButtonConfiguration
                            primaryAction:nil];
    [_unitMenuButton setTintColor:[UIColor colorNamed:kTextSecondaryColor]];
    _unitMenuButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentFill;
    [_unitMenuButton setImage:chevronUpDown forState:UIControlStateNormal];

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
      [_unitMenuButton.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kUnitMenuButtonLeadingOffset],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToAnchor:_unitMenuButton.heightAnchor
                                      constant:kContentViewHeightAnchorOffset],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToConstant:kUnitTypeCellHeightAnchor],
    ]];
  }
  return self;
}

@end
