// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
