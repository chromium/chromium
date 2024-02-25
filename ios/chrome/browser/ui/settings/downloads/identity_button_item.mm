// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/downloads/identity_button_item.h"

#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/settings/downloads/identity_button_cell.h"

namespace {

// Alpha value of IdentityButtonControl when disabled.
constexpr CGFloat kDisabledAlpha = 0.5;

}  // namespace

@implementation IdentityButtonItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [IdentityButtonCell class];
    _identityEmail = @"";
    _identityName = @"";
    _identityGaiaID = @"";
    _arrowDirection = IdentityButtonControlArrowDown;
    _identityViewStyle = IdentityViewStyleDefault;
    _enabled = YES;
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(IdentityButtonCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.identityButtonControl.arrowDirection = self.arrowDirection;
  cell.identityButtonControl.identityViewStyle = self.identityViewStyle;
  [cell.identityButtonControl setIdentityAvatar:self.identityAvatar];
  [cell.identityButtonControl setIdentityName:self.identityName
                                        email:self.identityEmail];
  cell.identityButtonControl.enabled = self.enabled;
  cell.identityButtonControl.alpha = self.enabled ? 1.0 : kDisabledAlpha;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
}

@end
