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

@implementation SnapshotStorageWrapper {
  SnapshotStorage* _snapshotStorage;
  LegacySnapshotStorage* _legacySnapshotStorage;
}

- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath
                         legacyPath:(const base::FilePath&)legacyPath {
  if ((self = [super init])) {
    if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
      NSURL* storageUrl = base::apple::FilePathToNSURL(storagePath);
      NSURL* legacyUrl = base::apple::FilePathToNSURL(legacyPath);
      _snapshotStorage =
          [[SnapshotStorage alloc] initWithStorageDirectoryUrl:storageUrl
                                            legacyDirectoryUrl:legacyUrl];
    } else {
      _legacySnapshotStorage =
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
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_snapshotStorage);
    [_snapshotStorage
        retrieveImageWithSnapshotID:[[SnapshotIDWrapper alloc]
                                        initWithSnapshotID:snapshotID]
                         completion:callback];
  } else {
    CHECK(_legacySnapshotStorage);
    [_legacySnapshotStorage retrieveImageForSnapshotID:snapshotID
                                              callback:callback];
  }
}

- (void)retrieveGreyImageForSnapshotID:(SnapshotID)snapshotID
                              callback:(void (^)(UIImage*))callback {
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_snapshotStorage);
    [_snapshotStorage
        retrieveGreyImageWithSnapshotID:[[SnapshotIDWrapper alloc]
                                            initWithSnapshotID:snapshotID]
                             completion:callback];
  } else {
    CHECK(_legacySnapshotStorage);
    [_legacySnapshotStorage retrieveGreyImageForSnapshotID:snapshotID
                                                  callback:callback];
  }
}

- (void)setImage:(UIImage*)image withSnapshotID:(SnapshotID)snapshotID {
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_snapshotStorage);
    [_snapshotStorage
          setImage:image
        snapshotID:[[SnapshotIDWrapper alloc] initWithSnapshotID:snapshotID]];
  } else {
    CHECK(_legacySnapshotStorage);
    [_legacySnapshotStorage setImage:image withSnapshotID:snapshotID];
  }
}

- (void)removeImageWithSnapshotID:(SnapshotID)snapshotID {
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_snapshotStorage);
    [_snapshotStorage
        removeImageWithSnapshotID:[[SnapshotIDWrapper alloc]
                                      initWithSnapshotID:snapshotID]];
  } else {
    CHECK(_legacySnapshotStorage);
    [_legacySnapshotStorage removeImageWithSnapshotID:snapshotID];
  }
}

- (void)removeAllImages {
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_snapshotStorage);
    [_snapshotStorage removeAllImages];
  } else {
    CHECK(_legacySnapshotStorage);
    [_legacySnapshotStorage removeAllImages];
  }
}

- (void)purgeImagesOlderThan:(base::Time)date
                     keeping:(const std::vector<SnapshotID>&)liveSnapshotIDs {
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_snapshotStorage);
    NSMutableArray<SnapshotIDWrapper*>* snapshotIDs =
        [[NSMutableArray alloc] initWithCapacity:liveSnapshotIDs.size()];
    for (auto& liveSnapshotID : liveSnapshotIDs) {
      [snapshotIDs addObject:[[SnapshotIDWrapper alloc]
                                 initWithSnapshotID:liveSnapshotID]];
    }
    [_snapshotStorage purgeImagesOlderThanWithThresholdDate:date.ToNSDate()
                                            liveSnapshotIDs:snapshotIDs];
  } else {
    CHECK(_legacySnapshotStorage);
    [_legacySnapshotStorage purgeImagesOlderThan:date keeping:liveSnapshotIDs];
  }
}

- (void)renameSnapshotsWithIDs:(NSArray<NSString*>*)oldIDs
                         toIDs:(const std::vector<SnapshotID>&)newIDs {
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_snapshotStorage);
    NSMutableArray<SnapshotIDWrapper*>* snapshotIDs =
        [[NSMutableArray alloc] initWithCapacity:newIDs.size()];
    for (auto& newID : newIDs) {
      [snapshotIDs
          addObject:[[SnapshotIDWrapper alloc] initWithSnapshotID:newID]];
    }
    [_snapshotStorage renameSnapshotsWithOldIDs:oldIDs newIDs:snapshotIDs];
  } else {
    CHECK(_legacySnapshotStorage);
    [_legacySnapshotStorage renameSnapshotsWithIDs:oldIDs toIDs:newIDs];
  }
}

- (void)migrateImageWithSnapshotID:(SnapshotID)snapshotID
                 toSnapshotStorage:(SnapshotStorageWrapper*)destinationStorage {
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_snapshotStorage);
    CHECK(destinationStorage->_snapshotStorage);
    [_snapshotStorage
        migrateImageWithSnapshotID:[[SnapshotIDWrapper alloc]
                                       initWithSnapshotID:snapshotID]
                destinationStorage:destinationStorage->_snapshotStorage];
  } else {
    CHECK(_legacySnapshotStorage);
    CHECK(destinationStorage->_legacySnapshotStorage);
    [_legacySnapshotStorage
        migrateImageWithSnapshotID:snapshotID
                 toSnapshotStorage:destinationStorage->_legacySnapshotStorage];
  }
}

- (void)addObserver:(id<SnapshotStorageObserver>)observer {
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_snapshotStorage);
    [_snapshotStorage addObserver:observer];
  } else {
    CHECK(_legacySnapshotStorage);
    [_legacySnapshotStorage addObserver:observer];
  }
}

- (void)removeObserver:(id<SnapshotStorageObserver>)observer {
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_snapshotStorage);
    [_snapshotStorage removeObserver:observer];
  } else {
    CHECK(_legacySnapshotStorage);
    [_legacySnapshotStorage removeObserver:observer];
  }
}

- (void)shutdown {
  // The new implementation doesn't have `-shutdown:`.
  if (!base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    CHECK(_legacySnapshotStorage);
    [_legacySnapshotStorage shutdown];
  }
}

@end
