// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/legacy_snapshot_storage.h"

#import <map>

#import "base/containers/contains.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_forward.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/legacy_image_file_manager.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_lru_cache.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_storage+Testing.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"

namespace {

// Maximum size in number of elements that the LRU cache can hold before
// starting to evict elements.
const NSUInteger kLRUCacheMaxCapacity = 6;

// Maximum size in number of elements that the LRU cache can hold before
// starting to evict elements when PinnedTabs feature is enabled.
//
// To calculate the cache size number we'll start with the assumption that
// currently snapshot preloading feature "works fine". In the reality it might
// not be the case for large screen devices such as iPad. Another assumption
// here is that pinned tabs feature requires on average 4 more snapshots to be
// used. Based on that kLRUCacheMaxCapacityForPinnedTabsEnabled is
// kLRUCacheMaxCapacity which "works fine" + on average 4 more snapshots needed
// for pinned tabs feature.
const NSUInteger kLRUCacheMaxCapacityForPinnedTabsEnabled = 10;

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

- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath
                         legacyPath:(const base::FilePath&)legacyPath {
  if ((self = [super init])) {
    NSUInteger cacheSize = IsPinnedTabsEnabled()
                               ? kLRUCacheMaxCapacityForPinnedTabsEnabled
                               : kLRUCacheMaxCapacity;
    _lruCache = [[LegacySnapshotLRUCache alloc] initWithCacheSize:cacheSize];

    _fileManager =
        [[LegacyImageFileManager alloc] initWithStoragePath:storagePath
                                                 legacyPath:legacyPath];

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
  return [self initWithStoragePath:storagePath legacyPath:base::FilePath()];
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

- (void)retrieveImageForSnapshotID:(SnapshotID)snapshotID
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

- (void)setImage:(UIImage*)image withSnapshotID:(SnapshotID)snapshotID {
  if (!image || !snapshotID.valid()) {
    return;
  }

  [_lruCache setObject:image forKey:snapshotID];

  // Each image in the cache has the same resolution and hence the same size.
  size_t imageSizes = CGImageGetBytesPerRow(image.CGImage) *
                      CGImageGetHeight(image.CGImage) * [_lruCache count];
  base::UmaHistogramMemoryKB("IOS.Snapshots.CacheSize", imageSizes / 1024);

  [self.observers
      didUpdateSnapshotStorageWithSnapshotID:
          [[SnapshotIDWrapper alloc] initWithSnapshotID:snapshotID]];

  // Save the image to disk.
  [_fileManager writeImage:image withSnapshotID:snapshotID];
}

- (void)removeImageWithSnapshotID:(SnapshotID)snapshotID {
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

- (void)purgeImagesOlderThan:(base::Time)date
                     keeping:(const std::vector<SnapshotID>&)liveSnapshotIDs {
  [_fileManager purgeImagesOlderThan:date keeping:liveSnapshotIDs];
}

- (void)renameSnapshotsWithIDs:(NSArray<NSString*>*)oldIDs
                         toIDs:(const std::vector<SnapshotID>&)newIDs {
  DCHECK_EQ(oldIDs.count, newIDs.size());
  [_fileManager renameSnapshotsWithIDs:oldIDs toIDs:newIDs];
}

- (void)migrateImageWithSnapshotID:(SnapshotID)snapshotID
                 toSnapshotStorage:(LegacySnapshotStorage*)destinationStorage {
  // Copy to the destination storage.
  if (UIImage* image = [_lruCache objectForKey:snapshotID]) {
    // Copy both on-disk and in-memory versions.
    [destinationStorage setImage:image withSnapshotID:snapshotID];
  } else {
    // Only copy on-disk.
    [_fileManager
        copyImage:[self imagePathForSnapshotID:snapshotID]
        toNewPath:[destinationStorage imagePathForSnapshotID:snapshotID]];
  }

  // Remove the snapshot from this storage.
  [self removeImageWithSnapshotID:snapshotID];
}

// Remove all UIImages from `lruCache_`.
- (void)handleLowMemory {
  [_lruCache removeAllObjects];
}

// Remove all UIImages from `lruCache_`.
- (void)handleEnterBackground {
  [_lruCache removeAllObjects];
}

- (void)addObserver:(id<SnapshotStorageObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<SnapshotStorageObserver>)observer {
  [self.observers removeObserver:observer];
}

- (void)shutdown {
  [_fileManager shutdown];
}

#pragma mark - Testing

- (base::FilePath)imagePathForSnapshotID:(SnapshotID)snapshotID {
  return [_fileManager imagePathForSnapshotID:snapshotID];
}

- (NSUInteger)lruCacheMaxSize {
  return [_lruCache maxCacheSize];
}

- (void)clearCache {
  [_lruCache removeAllObjects];
}

@end
