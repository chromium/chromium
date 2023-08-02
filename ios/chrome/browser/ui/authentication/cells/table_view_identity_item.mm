// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_item.h"

#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation TableViewIdentityItem

@synthesize gaiaID = _gaiaID;
@synthesize name = _name;
@synthesize email = _email;
@synthesize avatar = _avatar;
@synthesize selected = _selected;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewIdentityCell class];
    _identityViewStyle = IdentityViewStyleDefault;
    self.useCustomSeparator = YES;
  }
  return self;
}

- (void)configureCell:(TableViewIdentityCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  NSString* title = self.name;
  NSString* subtitle = self.email;
  if (!title.length) {
    title = subtitle;
    subtitle = nil;
  }
  cell.accessibilityIdentifier = self.email;
  [cell configureCellWithTitle:title
                      subtitle:subtitle
                         image:self.avatar
                       checked:self.selected
             identityViewStyle:self.identityViewStyle
                    titleColor:[UIColor colorNamed:kTextPrimaryColor]];
}

@end
