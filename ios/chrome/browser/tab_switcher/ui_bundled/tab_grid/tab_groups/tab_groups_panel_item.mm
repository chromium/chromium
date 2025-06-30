// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_item.h"

#import "base/uuid.h"

@implementation TabGroupsPanelItem {
  NSUInteger _hash;
}

- (instancetype)initWithOutOfDateMessage {
  self = [super init];
  if (self) {
    _type = TabGroupsPanelItemType::kOutOfDateMessage;
    // There is only one possible item in this case.
    _hash = 0;
  }
  return self;
}

- (instancetype)initWithNotificationText:(NSString*)text {
  self = [super init];
  if (self) {
    _type = TabGroupsPanelItemType::kNotification;
    _notificationText = [text copy];
    _hash = text.hash;
  }
  return self;
}

- (instancetype)initWithSavedTabGroupID:(base::Uuid)savedTabGroupID
                           sharingState:(tab_groups::SharingState)sharingState {
  self = [super init];
  if (self) {
    _type = TabGroupsPanelItemType::kSavedTabGroup;
    _savedTabGroupID = savedTabGroupID;
    _sharingState = sharingState;
    _hash = base::UuidHash()(_savedTabGroupID);
  }
  return self;
}

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
  return _hash;
}

#pragma mark Private

- (BOOL)isEqualToTabGroupsPanelItem:(TabGroupsPanelItem*)item {
  if (self == item) {
    return YES;
  }
  if (_type != item.type) {
    return NO;
  }
  if (_sharingState != item.sharingState) {
    return NO;
  }
  switch (_type) {
    case TabGroupsPanelItemType::kOutOfDateMessage:
      return YES;
    case TabGroupsPanelItemType::kNotification:
      return [self.notificationText isEqualToString:item.notificationText];
    case TabGroupsPanelItemType::kSavedTabGroup:
      return self.savedTabGroupID == item.savedTabGroupID;
  }
}

@end
