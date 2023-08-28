// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"

#import "base/time/time.h"
#import "url/gurl.h"

@implementation TabResumptionItem

- (instancetype)initWithItemType:(TabResumptionItemType)itemType {
  if ((self = [super init])) {
    _itemType = itemType;
  }
  return self;
}

@end
