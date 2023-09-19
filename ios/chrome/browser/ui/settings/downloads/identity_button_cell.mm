// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/downloads/identity_button_cell.h"

#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation IdentityButtonCell

@synthesize identityButtonControl = _identityButtonControl;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _identityButtonControl =
        [[IdentityButtonControl alloc] initWithFrame:self.contentView.frame];
    // Remove rounded corners.
    _identityButtonControl.layer.cornerRadius = 0;
    _identityButtonControl.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_identityButtonControl];
    AddSameConstraints(self.contentView, _identityButtonControl);
  }
  return self;
}

@end
