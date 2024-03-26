// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_ID_WRAPPER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_ID_WRAPPER_H_

#import <Foundation/Foundation.h>

#ifdef __cplusplus
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#endif

// A Objective-C wrapper for SnapshotID to make it usable from Swift.
@interface SnapshotIDWrapper : NSObject

#ifdef __cplusplus
- (instancetype)initWithSnapshotID:(SnapshotID)snapshot_id
    NS_DESIGNATED_INITIALIZER;
#endif
- (instancetype)initWithIdentifier:(int32_t)identifier
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

#ifdef __cplusplus
@property(nonatomic, readonly) SnapshotID snapshot_id;
#endif

@property(nonatomic, readonly) int32_t identifier;

// Returns true if SnapshotID is valid.
- (BOOL)valid;

// Returns the hash value of `identifier`.
- (NSUInteger)hash;

// Returns true if this SnapshotIDWrapper is equal to `other`.
- (BOOL)isEqual:(const SnapshotIDWrapper*)other;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_ID_WRAPPER_H_
