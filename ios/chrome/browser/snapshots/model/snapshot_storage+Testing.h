// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_TESTING_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_TESTING_H_

#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage.h"

namespace base {
class FilePath;
}

@class NSString;

// Private methods that should only be used for tests.
@interface SnapshotStorage (Testing)
// Returns the file path to the color snapshot of `snapshotID`.
- (base::FilePath)imagePathForSnapshotID:(SnapshotID)snapshotID;
// Returns the file path to the greyscale snapshot of `snapshotID`.
- (base::FilePath)greyImagePathForSnapshotID:(SnapshotID)snapshotID;
// Requests the grey snapshot for `snapshotID`. If the image is already loaded
// in memory, this will immediately call back with `callback`. Otherwise, only
// uses `callback` for the most recent caller. The callback is not guaranteed to
// be called.
- (void)greyImageForSnapshotID:(SnapshotID)snapshotID
                      callback:(void (^)(UIImage*))callback;
// Returns true if the cache for grey images has the image for `snapshotID`.
- (BOOL)hasGreyImageInMemory:(SnapshotID)snapshotID;
// Returns the max number of elements that the LRU cache can store.
- (NSUInteger)lruCacheMaxSize;
// Removes all elements in the LRU cache.
- (void)clearCache;
@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_TESTING_H_
