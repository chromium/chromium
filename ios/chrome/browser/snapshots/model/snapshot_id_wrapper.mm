// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"

#import "base/apple/foundation_util.h"

@implementation SnapshotIDWrapper {
  SnapshotID snapshot_id_;
}

- (instancetype)initWithSnapshotID:(SnapshotID)snapshot_id {
  if ((self = [super init])) {
    snapshot_id_ = snapshot_id;
  }
  return self;
}

- (instancetype)initWithIdentifier:(int32_t)identifier {
  if ((self = [super init])) {
    snapshot_id_ = SnapshotID(identifier);
  }
  return self;
}

- (SnapshotID)snapshot_id {
  return snapshot_id_;
}

- (int32_t)identifier {
  return snapshot_id_.identifier();
}

- (BOOL)valid {
  return snapshot_id_.valid();
}

- (NSUInteger)hash {
  return std::hash<int32_t>{}(self.identifier);
}

- (BOOL)isEqual:(NSObject*)other {
  if (![other isKindOfClass:[SnapshotIDWrapper class]]) {
    return NO;
  }

  return self.snapshot_id ==
         base::apple::ObjCCastStrict<SnapshotIDWrapper>(other).snapshot_id;
}

@end
