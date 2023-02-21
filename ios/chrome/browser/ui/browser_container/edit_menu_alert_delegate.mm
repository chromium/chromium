// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/edit_menu_alert_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation EditMenuAlertDelegateAction
- (instancetype)initWithTitle:(NSString*)title
                       action:(ProceduralBlock)action
                        style:(UIAlertActionStyle)style {
  self = [super init];
  if (self) {
    _title = [title copy];
    _action = action;
    _style = style;
  }
  return self;
}

@end
