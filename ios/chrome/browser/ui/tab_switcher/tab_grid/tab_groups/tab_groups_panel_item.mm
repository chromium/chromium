// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item.h"

#import "base/uuid.h"

@implementation TabGroupsPanelItem

#pragma mark NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[TabGroupsPanelItem class]]) {
    return NO;
  }
  return [self isEqualToTabGroupsPanelItem:object];
}

- (NSUInteger)hash {
  return base::UuidHash()(_savedTabGroupID);
}

#pragma mark Private

- (BOOL)isEqualToTabGroupsPanelItem:(TabGroupsPanelItem*)item {
  if (self == item) {
    return YES;
  }
  return _savedTabGroupID == item.savedTabGroupID;
}

@end
