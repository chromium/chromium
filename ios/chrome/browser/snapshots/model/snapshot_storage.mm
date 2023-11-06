// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_storage.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage+Testing.h"

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
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/image_file_manager.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_lru_cache.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_observer.h"
#import "ios/chrome/browser/tabs/model/features.h"

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

// Returns true if the flag for grey optimization is enabled.
bool IsGreySnapshotOptimizationEnabled() {
  if (base::FeatureList::IsEnabled(kGreySnapshotOptimization)) {
    return true;
  }
  return false;
}

// Returns true if the flag for grey optimization is enabled and the
// optimization level is highest, no grey snapshot images in in-memory cache and
// disk.
bool IsGreySnapshotOptimizationNoCacheEnabled() {
  if (IsGreySnapshotOptimizationEnabled()) {
    if (kGreySnapshotOptimizationLevelParam.Get() ==
        GreySnapshotOptimizationLevel::kDoNotStoreToDiskAndCache) {
      return true;
    }
  }
  return false;
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

@interface SnapshotStorage ()
// List of observers to be notified of changes to the snapshot storage.
@property(nonatomic, strong) SnapshotStorageObservers* observers;
@end

@implementation SnapshotStorage {
  // Cache to hold color snapshots in memory. n.b. Color snapshots are not
  // kept in memory on tablets.
  SnapshotLRUCache<UIImage*>* _lruCache;

  // File manager to read/write images from/to disk.
  __strong ImageFileManager* _fileManager;

  // Temporary dictionary to hold grey snapshots for tablet side swipe. This
  // will be nil before -createGreyCache is called and after -removeGreyCache
  // is called.
  std::map<SnapshotID, UIImage*> _greyImageDictionary;

  // Snapshot ID of most recent pending grey snapshot request.
  SnapshotID _mostRecentGreySnapshotID;
  // Block used by pending request for a grey snapshot.
  void (^_mostRecentGreyBlock)(UIImage*);

  // Snapshot ID and corresponding UIImage for the snapshot that will likely
  // be requested to be saved to disk when the application is backgrounded.
  SnapshotID _backgroundingSnapshotID;
  UIImage* _backgroundingColorImage;
}

- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath
                         legacyPath:(const base::FilePath&)legacyPath {
  if ((self = [super init])) {
    NSUInteger cacheSize = IsPinnedTabsEnabled()
                               ? kLRUCacheMaxCapacityForPinnedTabsEnabled
                               : kLRUCacheMaxCapacity;
    _lruCache = [[SnapshotLRUCache alloc] initWithCacheSize:cacheSize];

    _fileManager = [[ImageFileManager alloc] initWithStoragePath:storagePath
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
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
}

- (CGFloat)snapshotScaleForDevice {
  return [_fileManager snapshotScaleForDevice];
}

- (void)retrieveImageForSnapshotID:(SnapshotID)snapshotID
                          callback:(void (^)(UIImage*))callback {
  DCHECK(snapshotID.valid());
  DCHECK(callback);

  if (UIImage* image = [_lruCache objectForKey:snapshotID]) {
    callback(image);
    return;
  }

  __weak SnapshotLRUCache* weakLRUCache = _lruCache;
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

  auto iterator = _greyImageDictionary.find(snapshotID);
  if (iterator != _greyImageDictionary.end()) {
    CHECK(!IsGreySnapshotOptimizationNoCacheEnabled());
    callback(iterator->second);
    return;
  }

  if (IsGreySnapshotOptimizationEnabled()) {
    // There are no grey images stored in disk, so use color snapshots instead.
    UIImage* colorImage = [_lruCache objectForKey:snapshotID];
    if (colorImage) {
      callback(GreyImage(colorImage));
      return;
    }

    // Fallback to reading a color image from disk when there is no color image
    // in LRU cache.
    [_fileManager readImageWithSnapshotID:snapshotID
                               completion:base::BindOnce(^(UIImage* image) {
                                 if (image) {
                                   callback(GreyImage(image));
                                   return;
                                 }
                                 callback(nil);
                               })];
  } else {
    __weak SnapshotStorage* weakSelf = self;
    [_fileManager
        readGreyImageWithSnapshotID:snapshotID
                         completion:base::BindOnce(^(UIImage* image) {
                           if (image) {
                             callback(image);
                             return;
                           }
                           [weakSelf
                               retrieveImageForSnapshotID:snapshotID
                                                 callback:^(
                                                     UIImage* snapshotImage) {
                                                   if (snapshotImage) {
                                                     snapshotImage = GreyImage(
                                                         snapshotImage);
                                                   }
                                                   callback(snapshotImage);
                                                 }];
                         })];
  }
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

  [self.observers snapshotStorage:self didUpdateSnapshotForID:snapshotID];

  // Save the image to disk.
  [_fileManager writeImage:image withSnapshotID:snapshotID];
}

- (void)removeImageWithSnapshotID:(SnapshotID)snapshotID {
  [_lruCache removeObjectForKey:snapshotID];

  [self.observers snapshotStorage:self didUpdateSnapshotForID:snapshotID];

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
                 toSnapshotStorage:(SnapshotStorage*)destinationStorage {
  // Copy to the destination storage.
  if (UIImage* image = [_lruCache objectForKey:snapshotID]) {
    // Copy both on-disk and in-memory versions.
    [destinationStorage setImage:image withSnapshotID:snapshotID];
    // Copy the grey scale version, if available.
    auto iterator = _greyImageDictionary.find(snapshotID);
    if (iterator != _greyImageDictionary.end()) {
      DCHECK(!IsGreySnapshotOptimizationNoCacheEnabled());
      destinationStorage->_greyImageDictionary.insert(
          std::make_pair(snapshotID, iterator->second));
    }
  } else {
    // Only copy on-disk.
    [_fileManager
        copyImage:[self imagePathForSnapshotID:snapshotID]
        toNewPath:[destinationStorage imagePathForSnapshotID:snapshotID]];
    [_fileManager
        copyImage:[self greyImagePathForSnapshotID:snapshotID]
        toNewPath:[destinationStorage greyImagePathForSnapshotID:snapshotID]];
  }

  // Remove the snapshot from this storage.
  [self removeImageWithSnapshotID:snapshotID];
}

- (void)willBeSavedGreyWhenBackgrounding:(SnapshotID)snapshotID {
  if (!snapshotID.valid()) {
    return;
  }
  _backgroundingSnapshotID = snapshotID;
  _backgroundingColorImage = [_lruCache objectForKey:snapshotID];
}

// Remove all UIImages from `lruCache_`.
- (void)handleLowMemory {
  [_lruCache removeAllObjects];
}

// Remove all UIImages from `lruCache_`.
- (void)handleEnterBackground {
  [_lruCache removeAllObjects];
}

// Save grey image to `greyImageDictionary_` and call into most recent
// `_mostRecentGreyBlock` if `_mostRecentGreySnapshotID` matches `snapshotID`.
- (void)saveGreyImage:(UIImage*)greyImage forSnapshotID:(SnapshotID)snapshotID {
  CHECK(!IsGreySnapshotOptimizationNoCacheEnabled());
  if (greyImage) {
    _greyImageDictionary.insert(std::make_pair(snapshotID, greyImage));
  }
  if (snapshotID == _mostRecentGreySnapshotID) {
    _mostRecentGreyBlock(greyImage);
    [self clearGreySnapshotInfo];
  }
}

// Load uncached snapshot image and convert image to grey.
- (void)loadGreyImageAsync:(SnapshotID)snapshotID {
  CHECK(!IsGreySnapshotOptimizationNoCacheEnabled());
  // Use a color image in LRU cache if it exists.
  UIImage* cached_image = [_lruCache objectForKey:snapshotID];
  if (cached_image) {
    [self saveGreyImage:GreyImage(cached_image) forSnapshotID:snapshotID];
    return;
  }

  // Load a color image from disk and convert it into a grey image.
  __weak SnapshotStorage* weakSelf = self;
  [_fileManager readImageWithSnapshotID:snapshotID
                             completion:base::BindOnce(^(UIImage* image) {
                               if (image) {
                                 [weakSelf saveGreyImage:GreyImage(image)
                                           forSnapshotID:snapshotID];
                               }
                             })];
}

- (void)createGreyCache:(const std::vector<SnapshotID>&)snapshotIDs {
  if (IsGreySnapshotOptimizationNoCacheEnabled()) {
    // Do not create cache for grey images. A grey image will be generated
    // in-flight from a color image when it's retrieved.
    return;
  }

  _greyImageDictionary.clear();
  for (SnapshotID snapshotID : snapshotIDs) {
    [self loadGreyImageAsync:snapshotID];
  }
}

- (void)removeGreyCache {
  _greyImageDictionary.clear();
  [self clearGreySnapshotInfo];
}

// Clear most recent caller information.
- (void)clearGreySnapshotInfo {
  _mostRecentGreySnapshotID = SnapshotID();
  _mostRecentGreyBlock = nil;
}

- (void)saveGreyInBackgroundForSnapshotID:(SnapshotID)snapshotID {
  if (!snapshotID.valid()) {
    return;
  }

  // The color image may still be in memory.  Verify the snapshotID matches.
  if (_backgroundingColorImage) {
    if (snapshotID != _backgroundingSnapshotID) {
      _backgroundingSnapshotID = SnapshotID();
      _backgroundingColorImage = nil;
    }
  }

  if (IsGreySnapshotOptimizationEnabled()) {
    // Do not save grey images into disk when the feature is enabled.
    return;
  }

  [_fileManager convertAndSaveGreyImage:snapshotID];
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

- (base::FilePath)greyImagePathForSnapshotID:(SnapshotID)snapshotID {
  return [_fileManager greyImagePathForSnapshotID:snapshotID];
}

- (base::FilePath)legacyImagePathForSnapshotID:(NSString*)snapshotID {
  return [_fileManager legacyImagePathForSnapshotID:snapshotID];
}

- (void)greyImageForSnapshotID:(SnapshotID)snapshotID
                      callback:(void (^)(UIImage*))callback {
  DCHECK(snapshotID.valid());
  DCHECK(callback);

  auto iterator = _greyImageDictionary.find(snapshotID);
  if (iterator != _greyImageDictionary.end()) {
    callback(iterator->second);
    [self clearGreySnapshotInfo];
  } else {
    _mostRecentGreySnapshotID = snapshotID;
    _mostRecentGreyBlock = [callback copy];
  }
}

- (BOOL)hasGreyImageInMemory:(SnapshotID)snapshotID {
  return base::Contains(_greyImageDictionary, snapshotID);
}

- (NSUInteger)lruCacheMaxSize {
  return [_lruCache maxCacheSize];
}

- (void)clearCache {
  [_lruCache removeAllObjects];
}

@end
