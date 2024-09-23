// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_WRAPPER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_WRAPPER_H_

#import <UIKit/UIKit.h>

#include <vector>

class SnapshotID;
namespace base {
class FilePath;
class Time;
}  // namespace base

@protocol SnapshotStorageObserver;
@class SnapshotStorage;

// The wrapper class for LegacySnapshotStorage and SnapshotStorage.
// The APIs are exactly the same as LegacySnapshotStorage and the new
// implementation written in Swift is used when the flag (kSnapshotInSwift) is
// enabled.
// TODO(crbug.com/40943236): Remove this class once the new implementation
// written in Swift is used by default.
@interface SnapshotStorageWrapper : NSObject

@property(readonly) SnapshotStorage* snapshotStorage;

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
- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath
                         legacyPath:(const base::FilePath&)legacyPath
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer that pass an empty `legacyPath`.
- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath;
- (instancetype)init NS_UNAVAILABLE;

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

// Purges the storage of snapshots that are older than `date`. The snapshots for
// `liveSnapshotIDs` will be kept. This will be done asynchronously on a
// background thread.
- (void)purgeImagesOlderThan:(base::Time)date
                     keeping:(const std::vector<SnapshotID>&)liveSnapshotIDs;

// Renames snapshots with names in `oldIDs` to names in `newIDs`. It is a
// programmatic error if the two array do not have the same length.
- (void)renameSnapshotsWithIDs:(NSArray<NSString*>*)oldIDs
                         toIDs:(const std::vector<SnapshotID>&)newIDs;

// Moves the on-disk tab snapshot from the receiver storage to the destination
// on-disk storage. If the snapshot was also in-memory, it is moved as well.
- (void)migrateImageWithSnapshotID:(SnapshotID)snapshotID
                 toSnapshotStorage:(SnapshotStorageWrapper*)destinationCache;

// Adds an observer to this snapshot storage.
- (void)addObserver:(id<SnapshotStorageObserver>)observer;

// Removes an observer from this snapshot storage.
- (void)removeObserver:(id<SnapshotStorageObserver>)observer;

// Must be invoked before the instance is deallocated. It is needed to release
// all references to C++ objects. The receiver will likely soon be deallocated.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_WRAPPER_H_
