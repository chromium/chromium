// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_IMAGE_FILE_MANAGER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_IMAGE_FILE_MANAGER_H_

#include <vector>

#import <UIKit/UIKit.h>

#import "base/functional/callback_forward.h"

class SnapshotID;
namespace base {
class FilePath;
class Time;
}  // namespace base

using ImageReadCompletionBlock = base::OnceCallback<void(UIImage* image)>;

// A class to manage images stored in disk.
// TODO(crbug.com/40943236): Remove this class once the new implementation
// written in Swift is used by default.
@interface LegacyImageFileManager : NSObject

// Designated initializer. `storagePath` is the file path where all images
// managed by this ImageFileManager are stored. `storagePath` is not guaranteed
// to exist. The contents of `storagePath` are entirely managed by this
// ImageFileManager.
//
// To support renaming the directory where the snapshots are stored, it is
// possible to pass a non-empty path via `legacyPath`. If present, then it
// will be moved to `storagePath`.
//
// TODO(crbug.com/40942167): Remove `legacyPath` when the storage for all users
// has been migrated.
- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath
                         legacyPath:(const base::FilePath&)legacyPath
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Reads a color image from disk.
- (void)readImageWithSnapshotID:(SnapshotID)snapshotID
                     completion:(ImageReadCompletionBlock)completion;

// Writes an image to disk.
- (void)writeImage:(UIImage*)image withSnapshotID:(SnapshotID)snapshotID;

// Removes an image specified by `snapshotID` from disk.
- (void)removeImageWithSnapshotID:(SnapshotID)snapshotID;

// Removes all images from disk.
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

// Moves the image in disk from `oldPath` to `newPath`
- (void)copyImage:(const base::FilePath&)oldPath
        toNewPath:(const base::FilePath&)newPath;

// Returns the file path of the image for `snapshotID`.
- (base::FilePath)imagePathForSnapshotID:(SnapshotID)snapshotID;

// Returns the file path of the image for `snapshotID`.
// TODO(crbug.com/40942167): Remove this when the storage for all users has been
// migrated.
- (base::FilePath)legacyImagePathForSnapshotID:(NSString*)snapshotID;

// Must be invoked before the instance is deallocated. It is needed to release
// all references to C++ objects. The receiver will likely soon be deallocated.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_IMAGE_FILE_MANAGER_H_
