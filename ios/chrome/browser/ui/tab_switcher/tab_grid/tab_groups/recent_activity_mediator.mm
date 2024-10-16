// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_mediator.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_log_item.h"

@implementation RecentActivityMediator

- (void)setConsumer:(id<RecentActivityConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self populateItemsFromService];
  }
}

#pragma mark - Private

// Creates recent activity logs and passes them to the consumer.
- (void)populateItemsFromService {
  NSMutableArray<RecentActivityLogItem*>* items = [[NSMutableArray alloc] init];

  // TODO(crbug.com/370897655): Replace placeholder data with the actual data
  // from MessagingBackendService.
  for (int i = 0; i < 6; i++) {
    RecentActivityLogItem* item = [[RecentActivityLogItem alloc] init];
    item.type = static_cast<ActivityLogType>(i);
    item.favicon =
        DefaultSymbolTemplateWithPointSize(kXMarkCircleFillSymbol, 20);
    item.userIcon =
        DefaultSymbolTemplateWithPointSize(kXMarkCircleFillSymbol, 20);
    item.title = @"Test1 added a tab";
    item.actionDescription = @"google.com · 1h ago";
    item.timestamp = @"1h ago";
    [items addObject:item];
  }

  [_consumer populateItems:items];
}

@end
