// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/widget_context.h"

@implementation WidgetContext

- (instancetype)initWithContext:(UIOpenURLContext*)context
                         gaiaID:(NSString*)gaiaID
                           type:(AccountSwitchType)type {
  self = [super init];
  if (self) {
    _context = context;
    _gaiaID = gaiaID;
    _type = type;
  }
  return self;
}

@end
