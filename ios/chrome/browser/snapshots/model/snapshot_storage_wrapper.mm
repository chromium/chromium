// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_storage_wrapper.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/time/time.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_storage.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"

namespace {

// Converts `snapshot_id` to a SnapshotIDWrapper.
SnapshotIDWrapper* ToWrapper(SnapshotID snapshot_id) {
  return [[SnapshotIDWrapper alloc] initWithSnapshotID:snapshot_id];
}

// Converts `snapshot_ids` to an array of SnapshotIDWrappers.
NSArray<SnapshotIDWrapper*>* ToWrappers(
    const std::vector<SnapshotID> snapshot_ids) {
  NSMutableArray<SnapshotIDWrapper*>* wrappers = [[NSMutableArray alloc] init];
  for (SnapshotID snapshot_id : snapshot_ids) {
    [wrappers addObject:ToWrapper(snapshot_id)];
  }
  return wrappers;
}

}  // namespace

@implementation SnapshotStorageWrapper {
  id<SnapshotStorage> _snapshotStorage;
}

- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath
                         legacyPath:(const base::FilePath&)legacyPath {
  if ((self = [super init])) {
    if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
      NSURL* storageUrl = base::apple::FilePathToNSURL(storagePath);
      NSURL* legacyUrl = base::apple::FilePathToNSURL(legacyPath);
      _snapshotStorage =
          [[SnapshotStorageImpl alloc] initWithStorageDirectoryUrl:storageUrl
                                                legacyDirectoryUrl:legacyUrl];
    } else {
      _snapshotStorage =
          [[LegacySnapshotStorage alloc] initWithStoragePath:storagePath
                                                  legacyPath:legacyPath];
    }
  }
  return self;
}

- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath {
  return [self initWithStoragePath:storagePath legacyPath:base::FilePath()];
}

- (void)retrieveImageForSnapshotID:(SnapshotID)snapshotID
                          callback:(void (^)(UIImage*))callback {
  CHECK(_snapshotStorage);
  [_snapshotStorage retrieveImageWithSnapshotID:ToWrapper(snapshotID)
                                   snapshotKind:SnapshotKindColor
                                     completion:callback];
}

- (void)retrieveGreyImageForSnapshotID:(SnapshotID)snapshotID
                              callback:(void (^)(UIImage*))callback {
  CHECK(_snapshotStorage);
  [_snapshotStorage retrieveImageWithSnapshotID:ToWrapper(snapshotID)
                                   snapshotKind:SnapshotKindGreyscale
                                     completion:callback];
}

- (void)setImage:(UIImage*)image withSnapshotID:(SnapshotID)snapshotID {
  CHECK(_snapshotStorage);
  [_snapshotStorage setImage:image withSnapshotID:ToWrapper(snapshotID)];
}

- (void)removeImageWithSnapshotID:(SnapshotID)snapshotID {
  CHECK(_snapshotStorage);
  [_snapshotStorage removeImageWithSnapshotID:ToWrapper(snapshotID)];
}

- (void)removeAllImages {
  CHECK(_snapshotStorage);
  [_snapshotStorage removeAllImages];
}

- (void)purgeImagesOlderThan:(base::Time)date
                     keeping:(const std::vector<SnapshotID>&)liveSnapshotIDs {
  CHECK(_snapshotStorage);
  [_snapshotStorage
      purgeImagesOlderThanWithThresholdDate:date.ToNSDate()
                            liveSnapshotIDs:ToWrappers(liveSnapshotIDs)];
}

- (void)renameSnapshotsWithIDs:(NSArray<NSString*>*)oldIDs
                         toIDs:(const std::vector<SnapshotID>&)newIDs {
  CHECK(_snapshotStorage);
  [_snapshotStorage renameSnapshotsWithOldIDs:oldIDs newIDs:ToWrappers(newIDs)];
}

- (void)migrateImageWithSnapshotID:(SnapshotID)snapshotID
                 toSnapshotStorage:(SnapshotStorageWrapper*)destinationStorage {
  CHECK(_snapshotStorage);
  CHECK(destinationStorage->_snapshotStorage);
  [_snapshotStorage
      migrateImageWithSnapshotID:ToWrapper(snapshotID)
              destinationStorage:destinationStorage->_snapshotStorage];
}

- (void)addObserver:(id<SnapshotStorageObserver>)observer {
  CHECK(_snapshotStorage);
  [_snapshotStorage addObserver:observer];
}

- (void)removeObserver:(id<SnapshotStorageObserver>)observer {
  CHECK(_snapshotStorage);
  [_snapshotStorage removeObserver:observer];
}

- (void)shutdown {
  CHECK(_snapshotStorage);
  [_snapshotStorage shutdown];
}

@end
