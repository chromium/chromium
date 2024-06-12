// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"

#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_item.h"

@implementation AccountPickerSelectionScreenIdentityItemConfigurator

- (void)configureIdentityChooser:(TableViewIdentityItem*)item {
  item.gaiaID = self.gaiaID;
  item.name = self.name;
  item.email = self.email;
  item.avatar = self.avatar;
  item.selected = self.selected;
  item.useCustomSeparator = NO;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"<%@: %p, email: %@>",
                                    NSStringFromClass(self.class), self,
                                    self.email];
}

@end
