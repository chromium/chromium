// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/set_up_list_item.h"

#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"

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
