// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_log_item.h"

#import "base/apple/foundation_util.h"
#import "base/uuid.h"
#import "components/collaboration/public/messaging/activity_log.h"
#import "components/collaboration/public/messaging/message.h"

@implementation RecentActivityLogItem {
  NSUInteger _hash;
}

- (void)setActivityMetadata:
    (collaboration::messaging::MessageAttribution)activityMetadata {
  _activityMetadata = activityMetadata;
  if (activityMetadata.id.has_value()) {
    _hash = base::UuidHash()(activityMetadata.id.value());
  } else {
    _hash = 0;
  }
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:RecentActivityLogItem.class]) {
    return NO;
  }
  RecentActivityLogItem* otherLog =
      base::apple::ObjCCastStrict<RecentActivityLogItem>(object);
  if (!self.activityMetadata.id.has_value() ||
      !otherLog.activityMetadata.id.has_value()) {
    return NO;
  }
  return self.activityMetadata.id.value() ==
         otherLog.activityMetadata.id.value();
}

- (NSUInteger)hash {
  return _hash;
}

@end
