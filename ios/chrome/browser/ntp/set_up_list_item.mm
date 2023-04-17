// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/set_up_list_item.h"

#import "ios/chrome/browser/ntp/set_up_list_item_type.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SetUpListItem

- (instancetype)initWithType:(SetUpListItemType)type complete:(BOOL)complete {
  self = [super init];
  if (self) {
    _type = type;
    _complete = complete;
  }
  return self;
}

- (void)markComplete {
  _complete = YES;
}

@end
