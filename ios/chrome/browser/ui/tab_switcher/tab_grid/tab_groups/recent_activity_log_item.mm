// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_log_item.h"

@implementation RecentActivityLogItem

#pragma mark NSObject

// Returns true if 2 objects are RecentActivityLogItem and have the same ID.
- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[RecentActivityLogItem class]]) {
    return NO;
  }
  return [self isEqualToRecentActivityLogItem:object];
}

#pragma mark Private

// Returns true if 2 RecentActivityLogItems have the same ID.
- (BOOL)isEqualToRecentActivityLogItem:(RecentActivityLogItem*)item {
  // TODO(crbug.com/370897655): Check if 2 RecentActivityLogItems are equal
  // based on the ID in the ActivityLogItem struct.
  return NO;
}

@end
