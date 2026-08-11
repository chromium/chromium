// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/clear_browsing_data/ui/table_view_pop_up_cell.h"

#import "ios/chrome/browser/settings/clear_browsing_data/public/quick_delete_constants.h"
#import "ios/chrome/browser/settings/clear_browsing_data/ui/pop_up_menu_control.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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

    AddSameConstraints(_menuControl, self.contentView);
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
