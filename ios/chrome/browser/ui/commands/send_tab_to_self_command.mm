// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/send_tab_to_self_command.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SendTabToSelfCommand

@synthesize targetDeviceID = _targetDeviceID;
@synthesize targetDeviceName = _targetDeviceName;

- (instancetype)initWithTargetDeviceID:(NSString*)targetDeviceID
                      targetDeviceName:(NSString*)targetDeviceName {
  if (self = [super init]) {
    _targetDeviceID = [targetDeviceID copy];
    _targetDeviceName = [targetDeviceName copy];
  }
  return self;
}

@end