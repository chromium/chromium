// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/edit_menu_alert_delegate.h"

@implementation EditMenuAlertDelegateAction
- (instancetype)initWithTitle:(NSString*)title
                       action:(ProceduralBlock)action
                        style:(UIAlertActionStyle)style
                    preferred:(BOOL)preferred {
  self = [super init];
  if (self) {
    _title = [title copy];
    _action = action;
    _style = style;
    _preferred = preferred;
  }
  return self;
}

@end
