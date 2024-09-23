// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/table_view_pop_up_cell.h"

#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/pop_up_menu_control.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation TableViewPopUpCell {
  PopUpMenuControl* _menuControl;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    _menuControl = [[PopUpMenuControl alloc] init];
    _menuControl.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_menuControl];

    [NSLayoutConstraint activateConstraints:@[
      [_menuControl.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [_menuControl.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],
      [_menuControl.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor],
      [_menuControl.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
    ]];
  }
  return self;
}

#pragma mark - Properties

- (void)setMenu:(UIMenu*)menu {
  _menuControl.menu = menu;
}

- (void)setTitle:(NSString*)title {
  _menuControl.title = title;
}

@end
