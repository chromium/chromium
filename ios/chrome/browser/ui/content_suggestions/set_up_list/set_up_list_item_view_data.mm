// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"

enum class SetUpListItemType;

@implementation SetUpListItemViewData

- (instancetype)initWithType:(SetUpListItemType)type complete:(BOOL)complete {
  self = [super init];
  if (self) {
    _type = type;
    _complete = complete;
  }
  return self;
}

@end
