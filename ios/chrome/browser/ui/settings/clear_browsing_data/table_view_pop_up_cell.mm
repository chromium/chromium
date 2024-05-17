// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/table_view_pop_up_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Height/trailing/leading constraints of the label and the button.
constexpr CGFloat kLeadingOffset = 20;
constexpr CGFloat kHeightOffset = 10;
constexpr CGFloat kTrailingOffset = 16;

// Cells height anchors constraint.
constexpr CGFloat kCellHeightAnchor = 40;

// ContentView's height anchor offset to add some space is case the font becomes
// too big.
constexpr CGFloat kContentViewHeightAnchorOffset = 5;

}  // namespace

#pragma mark - TableViewPopUpCell

@implementation TableViewPopUpCell {
  UIButton* _menuButton;
  UILabel* _textLabel;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textLabel.numberOfLines = 1;
    [self.contentView addSubview:_textLabel];

    UIButtonConfiguration* unitMenuButtonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    unitMenuButtonConfiguration.contentInsets =
        NSDirectionalEdgeInsetsMake(0, 0, 0, 0);
    _menuButton = [UIButton buttonWithConfiguration:unitMenuButtonConfiguration
                                      primaryAction:nil];
    _menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_menuButton setTintColor:[UIColor colorNamed:kTextSecondaryColor]];
    _menuButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentLeft;
    _menuButton.accessibilityIdentifier = kQuickDeletePopUpButtonIdentifier;

    [self.contentView addSubview:_menuButton];
    [NSLayoutConstraint activateConstraints:@[
      // Center elements vertically.
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_menuButton.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // Make the label be on the LHS of the button.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kLeadingOffset],
      [_menuButton.leadingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor
                         constant:kLeadingOffset],
      [_menuButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTrailingOffset],

      // Make sure that the cell is always bigger than any of its contents.
      [_textLabel.heightAnchor
          constraintEqualToAnchor:self.contentView.heightAnchor
                         constant:-kHeightOffset],
      [_menuButton.heightAnchor
          constraintEqualToAnchor:self.contentView.heightAnchor
                         constant:-kHeightOffset],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToAnchor:_textLabel.heightAnchor
                                      constant:kContentViewHeightAnchorOffset],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToAnchor:_menuButton.heightAnchor
                                      constant:kContentViewHeightAnchorOffset],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToConstant:kCellHeightAnchor],
    ]];

    // TODO(crbug.com/341077648): Trigger the UIMenu when the entire cell is
    // tapped, not just the UIButton.
  }
  return self;
}

- (void)setMenu:(UIMenu*)menu {
  _menuButton.showsMenuAsPrimaryAction = YES;
  _menuButton.changesSelectionAsPrimaryAction = YES;
  _menuButton.menu = menu;
}

- (void)setTitle:(NSString*)title {
  _textLabel.text = title;
}

@end
