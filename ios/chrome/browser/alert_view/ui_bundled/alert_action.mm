// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"

@implementation AlertAction

- (instancetype)initWithTitle:(NSString*)title
                        style:(UIAlertActionStyle)style
                      handler:(void (^)(AlertAction* action))handler {
  self = [super init];
  if (self) {
    static NSInteger actionIdentifier = 0;
    _uniqueIdentifier = ++actionIdentifier;
    _title = [title copy];
    _handler = handler;
    _style = style;
  }
  return self;
}

+ (instancetype)actionWithTitle:(NSString*)title
                          style:(UIAlertActionStyle)style
                        handler:(void (^)(AlertAction* action))handler {
  return [[AlertAction alloc] initWithTitle:title style:style handler:handler];
}

@end
