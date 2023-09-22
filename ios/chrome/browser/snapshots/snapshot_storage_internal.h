// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_STORAGE_INTERNAL_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_STORAGE_INTERNAL_H_

#import "ios/chrome/browser/snapshots/snapshot_storage_internal.h"

namespace base {
class FilePath;
}

@class NSString;

@interface SnapshotStorage (Internal)
// Returns filepath to the color snapshot of `snapshotID`.
- (base::FilePath)imagePathForSnapshotID:(SnapshotID)snapshotID;
// Returns filepath to the greyscale snapshot of `snapshotID`.
- (base::FilePath)greyImagePathForSnapshotID:(SnapshotID)snapshotID;
// Returns filepath to the legacy color snapshot of `snapshotID`.
- (base::FilePath)legacyImagePathForSnapshotID:(NSString*)snapshotID;
@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_STORAGE_INTERNAL_H_
