// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_STORAGE_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_STORAGE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/snapshots/model/model_swift.h"

namespace base {
class FilePath;
}  // namespace base

@class LegacySnapshotLRUCache;
@protocol SnapshotStorageObserver;

// A class providing an in-memory and on-disk storage of tab snapshots.
// A snapshot is a full-screen image of the contents of the page at the current
// scroll offset and zoom level, used to stand in for the WKWebView if it has
// been purged from memory or when quickly switching tabs.
// Persists to disk on a background thread each time a snapshot changes.
// TODO(crbug.com/40943236): Remove this class once the new implementation
// written in Swift is used by default.
@interface LegacySnapshotStorage : NSObject <SnapshotStorage>

// Designated initializer. `storagePath` is the file path where all images
// managed by this LegacySnapshotStorage are stored. `storagePath` is not
// guaranteed to exist. The contents of `storagePath` are entirely managed by
// this LegacySnapshotStorage.
//
// To support renaming the directory where the snapshots are stored, it is
// possible to pass a non-empty path via `legacyPath`. If present, then it
// will be moved to `storagePath`.
//
// TODO(crbug.com/40942167): Remove when the storage for all users has been
// migrated.
- (instancetype)initWithLRUCache:(LegacySnapshotLRUCache*)lruCache
                     storagePath:(const base::FilePath&)storagePath
                      legacyPath:(const base::FilePath&)legacyPath
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer that uses a default `lruCache` .
- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath
                         legacyPath:(const base::FilePath&)legacyPath;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_STORAGE_H_
