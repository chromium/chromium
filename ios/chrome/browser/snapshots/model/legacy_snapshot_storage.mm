// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/legacy_snapshot_storage.h"

#import <map>

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/legacy_image_file_manager.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_lru_cache.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"

namespace {

// Base size in number of elements that the LRU cache can hold before
// starting to evict elements.
const NSUInteger kLRUCacheBaseCapacity = 6;

// Additional capacity of elements that the LRU cache can hold before starting
// to evict elements when PinnedTabs feature is enabled.
//
// To calculate the cache size number we'll start with the assumption that
// currently snapshot preloading feature "works fine". In the reality it might
// not be the case for large screen devices such as iPad. Another assumption
// here is that pinned tabs feature requires on average 4 more snapshots to be
// used. Based on that kLRUCacheMaxCapacityForPinnedTabsEnabled is
// kLRUCacheMaxCapacity which "works fine" + on average 4 more snapshots needed
// for pinned tabs feature.
const NSUInteger kLRUCacheAdditionalCapacityForPinnedTabsEnabled = 4;

// Convert `wrappers` to a vector of SnapshotID.
std::vector<SnapshotID> ToSnapshotIDs(NSArray<SnapshotIDWrapper*>* wrappers) {
  std::vector<SnapshotID> snapshot_ids;
  snapshot_ids.reserve(wrappers.count);
  for (SnapshotIDWrapper* wrapper in wrappers) {
    snapshot_ids.push_back(wrapper.snapshot_id);
  }
  return snapshot_ids;
}

// Returns a default LegacySnapshotLRUCache instance.
LegacySnapshotLRUCache<UIImage*>* CreateDefaultSnapshotLRUCache() {
  NSUInteger cacheSize = kLRUCacheBaseCapacity;
  if (IsPinnedTabsEnabled()) {
    cacheSize += kLRUCacheAdditionalCapacityForPinnedTabsEnabled;
  }
  return [[LegacySnapshotLRUCache alloc] initWithCacheSize:cacheSize];
}

}  // namespace

// Protocol observers subclass that explicitly implements
// <SnapshotStorageObserver>.
@interface SnapshotStorageObservers
    : CRBProtocolObservers <SnapshotStorageObserver>
+ (instancetype)observers;
@end

@implementation SnapshotStorageObservers
+ (instancetype)observers {
  return [self observersWithProtocol:@protocol(SnapshotStorageObserver)];
}
@end

@interface LegacySnapshotStorage ()
// List of observers to be notified of changes to the snapshot storage.
@property(nonatomic, strong) SnapshotStorageObservers* observers;
@end

@implementation LegacySnapshotStorage {
  // Cache to hold color snapshots in memory. n.b. Color snapshots are not
  // kept in memory on tablets.
  LegacySnapshotLRUCache<UIImage*>* _lruCache;

  // File manager to read/write images from/to disk.
  __strong LegacyImageFileManager* _fileManager;
}

- (instancetype)initWithLRUCache:(LegacySnapshotLRUCache*)lruCache
                     storagePath:(const base::FilePath&)storagePath {
  if ((self = [super init])) {
    _lruCache = lruCache;
    _fileManager =
        [[LegacyImageFileManager alloc] initWithStoragePath:storagePath];

    _observers = [SnapshotStorageObservers observers];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleLowMemory)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleEnterBackground)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];
  }

  return self;
}

- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath {
  return [self initWithLRUCache:CreateDefaultSnapshotLRUCache()
                    storagePath:storagePath];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidReceiveMemoryWarningNotification
              object:nil];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidEnterBackgroundNotification
              object:nil];
}

#pragma mark - SnapshotStorage

- (void)retrieveImageWithSnapshotID:(SnapshotIDWrapper*)snapshotID
                       snapshotKind:(SnapshotKind)snapshotKind
                         completion:(void (^)(UIImage*))completion {
  switch (snapshotKind) {
    case SnapshotKindColor:
      [self retrieveColorImageForSnapshotID:snapshotID.snapshot_id
                                   callback:completion];
      break;

    case SnapshotKindGreyscale:
      [self retrieveGreyImageForSnapshotID:snapshotID.snapshot_id
                                  callback:completion];
      break;
  }
}

- (void)setImage:(UIImage*)image
    withSnapshotID:(SnapshotIDWrapper*)snapshotIDWrapper {
  const SnapshotID snapshotID = snapshotIDWrapper.snapshot_id;
  if (!image || !snapshotID.valid()) {
    return;
  }

  [_lruCache setObject:image forKey:snapshotID];

  [self.observers didUpdateSnapshotStorageWithSnapshotID:snapshotIDWrapper];

  // Save the image to disk.
  [_fileManager writeImage:image withSnapshotID:snapshotID];
}

- (void)removeImageWithSnapshotID:(SnapshotIDWrapper*)snapshotIDWrapper {
  const SnapshotID snapshotID = snapshotIDWrapper.snapshot_id;
  [_lruCache removeObjectForKey:snapshotID];

  [self.observers
      didUpdateSnapshotStorageWithSnapshotID:
          [[SnapshotIDWrapper alloc] initWithSnapshotID:snapshotID]];

  [_fileManager removeImageWithSnapshotID:snapshotID];
}

- (void)removeAllImages {
  [_lruCache removeAllObjects];

  [_fileManager removeAllImages];
}

- (void)purgeImagesOlderThanWithThresholdDate:(NSDate*)thresholdDate
                              liveSnapshotIDs:(NSArray<SnapshotIDWrapper*>*)
                                                  liveSnapshotIDs {
  [_fileManager purgeImagesOlderThan:base::Time::FromNSDate(thresholdDate)
                             keeping:ToSnapshotIDs(liveSnapshotIDs)];
}

- (void)migrateImageWithSnapshotID:(SnapshotIDWrapper*)snapshotIDWrapper
                destinationStorage:(id<SnapshotStorage>)destinationStorage {
  const SnapshotID snapshotID = snapshotIDWrapper.snapshot_id;
  // Copy to the destination storage.
  if (UIImage* image = [_lruCache objectForKey:snapshotID]) {
    // Copy both on-disk and in-memory versions.
    [destinationStorage setImage:image withSnapshotID:snapshotIDWrapper];
  } else {
    // Only copy on-disk.
    [_fileManager copyImage:base::apple::NSURLToFilePath([self
                                imagePathWithSnapshotID:snapshotIDWrapper])
                  toNewPath:base::apple::NSURLToFilePath([destinationStorage
                                imagePathWithSnapshotID:snapshotIDWrapper])];
  }

  // Remove the snapshot from this storage.
  [self removeImageWithSnapshotID:snapshotIDWrapper];
}

- (void)addObserver:(id<SnapshotStorageObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<SnapshotStorageObserver>)observer {
  [self.observers removeObserver:observer];
}

- (NSURL*)imagePathWithSnapshotID:(SnapshotIDWrapper*)snapshotID {
  return base::apple::FilePathToNSURL(
      [_fileManager imagePathForSnapshotID:snapshotID.snapshot_id]);
}

- (void)shutdown {
  [_fileManager shutdown];
}

#pragma mark - Private methods

- (void)retrieveColorImageForSnapshotID:(SnapshotID)snapshotID
                               callback:(void (^)(UIImage*))callback {
  DCHECK(snapshotID.valid());
  DCHECK(callback);

  if (UIImage* image = [_lruCache objectForKey:snapshotID]) {
    callback(image);
    return;
  }

  __weak LegacySnapshotLRUCache* weakLRUCache = _lruCache;
  [_fileManager readImageWithSnapshotID:snapshotID
                             completion:base::BindOnce(^(UIImage* image) {
                               if (image) {
                                 [weakLRUCache setObject:image
                                                  forKey:snapshotID];
                               }
                               callback(image);
                             })];
}

- (void)retrieveGreyImageForSnapshotID:(SnapshotID)snapshotID
                              callback:(void (^)(UIImage*))callback {
  DCHECK(snapshotID.valid());
  DCHECK(callback);

  // There are no grey images stored in disk, so generate it from a colored
  // snapshot if exists.
  UIImage* colorImage = [_lruCache objectForKey:snapshotID];
  if (colorImage) {
    callback(GreyImage(colorImage));
    return;
  }

  // Fallback to reading a color image from disk when there is no color image in
  // LRU cache.
  [_fileManager readImageWithSnapshotID:snapshotID
                             completion:base::BindOnce(^(UIImage* image) {
                               if (image) {
                                 callback(GreyImage(image));
                                 return;
                               }
                               callback(nil);
                             })];
}

// Remove all UIImages from `lruCache_`.
- (void)handleLowMemory {
  [_lruCache removeAllObjects];
}

// Remove all UIImages from `lruCache_`.
- (void)handleEnterBackground {
  [_lruCache removeAllObjects];
}

@end
