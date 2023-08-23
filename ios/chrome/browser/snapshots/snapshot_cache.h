// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_H_

#include <vector>

#import <UIKit/UIKit.h>

class SnapshotID;
namespace base {
class FilePath;
class Time;
}

@protocol SnapshotCacheObserver;

// A class providing an in-memory and on-disk cache of tab snapshots.
// A snapshot is a full-screen image of the contents of the page at the current
// scroll offset and zoom level, used to stand in for the WKWebView if it has
// been purged from memory or when quickly switching tabs.
// Persists to disk on a background thread each time a snapshot changes.
@interface SnapshotCache : NSObject

// Designated initializer. `storagePath` is the file path where all images
// managed by this SnapshotCache are stored. `storagePath` is not guaranteed to
// exist. The contents of `storagePath` are entirely managed by this
// SnapshotCache.
//
// To support renaming the directory where the snapshots are stored, it is
// possible to pass a non-empty path via `legacyPath`. If present, then it
// will be moved to `storagePath`.
//
// TODO(crbug.com/1383087): Remove when the storage for all users has been
// migrated.
- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath
                         legacyPath:(const base::FilePath&)legacyPath
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer that pass an empty `legacyPath`.
- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath;
- (instancetype)init NS_UNAVAILABLE;

// The scale that should be used for snapshots.
- (CGFloat)snapshotScaleForDevice;

// Retrieves a cached snapshot for the `snapshotID` and return it via the
// callback if it exists. The callback is guaranteed to be called synchronously
// if the image is in memory. It will be called asynchronously if the image is
// on disk or with nil if the image is not present at all.
- (void)retrieveImageForSnapshotID:(SnapshotID)snapshotID
                          callback:(void (^)(UIImage*))callback;

// Requests the grey snapshot for `snapshotID`. If the image is already loaded
// in memory, this will immediately call back with `callback`.
- (void)retrieveGreyImageForSnapshotID:(SnapshotID)snapshotID
                              callback:(void (^)(UIImage*))callback;

// Sets the image in both the LRU and disk.
- (void)setImage:(UIImage*)image withSnapshotID:(SnapshotID)snapshotID;

// Removes the image from both the LRU and disk.
- (void)removeImageWithSnapshotID:(SnapshotID)snapshotID;

// Removes all images from both the LRU and disk.
- (void)removeAllImages;

// Purges the cache of snapshots that are older than `date`. The snapshots for
// `liveSnapshotIDs` will be kept. This will be done asynchronously on a
// background thread.
- (void)purgeCacheOlderThan:(base::Time)date
                    keeping:(const std::vector<SnapshotID>&)liveSnapshotIDs;

// Renames snapshots with names in `oldIDs` to names in `newIDs`. It is a
// programmatic error if the two array do not have the same length.
- (void)renameSnapshotsWithIDs:(NSArray<NSString*>*)oldIDs
                         toIDs:(const std::vector<SnapshotID>&)newIDs;

// Moves the on-disk tab snapshot from the receiver cache to the destination
// on-disk cache. If the snapshot was also in-memory, it is moved as well. The
// grey image is handled as well if present.
- (void)migrateImageWithSnapshotID:(SnapshotID)snapshotID
                   toSnapshotCache:(SnapshotCache*)destinationCache;

// Hints that the snapshot for `snapshotID` will likely be saved to disk when
// the application is backgrounded.  The snapshot is then saved in memory, so it
// does not need to be read off disk.
- (void)willBeSavedGreyWhenBackgrounding:(SnapshotID)snapshotID;

// Creates temporary cache of grey images for tablet side-swipe.
- (void)createGreyCache:(const std::vector<SnapshotID>&)snapshotIDs;

// Releases alls images in grey cache.
- (void)removeGreyCache;

// Writes a grey copy of the snapshot for `snapshotID` to disk, but if and only
// if a color version of the snapshot already exists in memory or on disk.
- (void)saveGreyInBackgroundForSnapshotID:(SnapshotID)snapshotID;

// Adds an observer to this snapshot cache.
- (void)addObserver:(id<SnapshotCacheObserver>)observer;

// Removes an observer from this snapshot cache.
- (void)removeObserver:(id<SnapshotCacheObserver>)observer;

// Must be invoked before the instance is deallocated. It is needed to release
// all references to C++ objects. The receiver will likely soon be deallocated.
- (void)shutdown;

@end

// Additionnal methods that should only be used for tests.
@interface SnapshotCache (TestingAdditions)
// Requests the grey snapshot for `snapshotID`. If the image is already loaded
// in memory, this will immediately call back with `callback`. Otherwise, only
// uses `callback` for the most recent caller. The callback is not guaranteed to
// be called.
- (void)greyImageForSnapshotID:(SnapshotID)snapshotID
                      callback:(void (^)(UIImage*))callback;
- (BOOL)hasImageInMemory:(SnapshotID)snapshotID;
- (BOOL)hasGreyImageInMemory:(SnapshotID)snapshotID;
- (NSUInteger)lruCacheMaxSize;
@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_H_
