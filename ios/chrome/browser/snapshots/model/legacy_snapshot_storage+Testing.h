// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_STORAGE_TESTING_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_STORAGE_TESTING_H_

#import "ios/chrome/browser/snapshots/model/legacy_snapshot_storage.h"

class SnapshotID;
namespace base {
class FilePath;
}

@class NSString;

// Private methods that should only be used for tests.
// TODO(crbug.com/40943236): Remove this class once the new implementation
// written in Swift is used by default.
@interface LegacySnapshotStorage (Testing)
// Returns the file path to the color snapshot of `snapshotID`.
- (base::FilePath)imagePathForSnapshotID:(SnapshotID)snapshotID;
// Returns the max number of elements that the LRU cache can store.
- (NSUInteger)lruCacheMaxSize;
// Removes all elements in the LRU cache.
- (void)clearCache;
@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_STORAGE_TESTING_H_
