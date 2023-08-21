// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_internal.h"

#import <UIKit/UIKit.h>

#import <set>

#import "base/apple/backup_util.h"
#import "base/base_paths.h"
#import "base/containers/contains.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/path_service.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_observer.h"
#import "ios/chrome/browser/snapshots/snapshot_lru_cache.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ui/base/device_form_factor.h"

// Protocol observers subclass that explicitly implements
// <SnapshotCacheObserver>.
@interface SnapshotCacheObservers : CRBProtocolObservers<SnapshotCacheObserver>
+ (instancetype)observers;
@end

@implementation SnapshotCacheObservers
+ (instancetype)observers {
  return [self observersWithProtocol:@protocol(SnapshotCacheObserver)];
}
@end

@interface SnapshotCache ()
// List of observers to be notified of changes to the snapshot cache.
@property(nonatomic, strong) SnapshotCacheObservers* observers;
@end

namespace {
enum ImageType {
  IMAGE_TYPE_COLOR,
  IMAGE_TYPE_GREYSCALE,
};

enum ImageScale {
  IMAGE_SCALE_1X,
  IMAGE_SCALE_2X,
  IMAGE_SCALE_3X,
};

const ImageType kImageTypes[] = {
    IMAGE_TYPE_COLOR, IMAGE_TYPE_GREYSCALE,
};

const NSUInteger kGreyInitialCapacity = 8;
const CGFloat kJPEGImageQuality = 1.0;  // Highest quality. No compression.

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

// Returns the path of the image for `snapshot_id`, in `cache_directory`,
// of type `image_type` and scale `image_scale`.
base::FilePath ImagePath(NSString* snapshot_id,
                         ImageType image_type,
                         ImageScale image_scale,
                         const base::FilePath& cache_directory) {
  NSString* filename = snapshot_id;
  switch (image_type) {
    case IMAGE_TYPE_COLOR:
      // no-op
      break;
    case IMAGE_TYPE_GREYSCALE:
      filename = [filename stringByAppendingString:@"Grey"];
      break;
  }
  switch (image_scale) {
    case IMAGE_SCALE_1X:
      // no-op
      break;
    case IMAGE_SCALE_2X:
      filename = [filename stringByAppendingString:@"@2x"];
      break;
    case IMAGE_SCALE_3X:
      filename = [filename stringByAppendingString:@"@3x"];
      break;
  }
  filename = [filename stringByAppendingPathExtension:@"jpg"];
  return cache_directory.Append(base::SysNSStringToUTF8(filename));
}

ImageScale ImageScaleForDevice() {
  // On handset, the color snapshot is used for the stack view, so the scale of
  // the snapshot images should match the scale of the device.
  // On tablet, the color snapshot is only used to generate the grey snapshot,
  // which does not have to be high quality, so use scale of 1.0 on all tablets.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
    return IMAGE_SCALE_1X;

  // Cap snapshot resolution to 2x to reduce the amount of memory used.
  return [UIScreen mainScreen].scale == 1.0 ? IMAGE_SCALE_1X : IMAGE_SCALE_2X;
}

CGFloat ScaleFromImageScale(ImageScale image_scale) {
  switch (image_scale) {
    case IMAGE_SCALE_1X:
      return 1.0;
    case IMAGE_SCALE_2X:
      return 2.0;
    case IMAGE_SCALE_3X:
      return 3.0;
  }
}

UIImage* ReadImageForSnapshotIDFromDisk(NSString* snapshot_id,
                                        ImageType image_type,
                                        ImageScale image_scale,
                                        const base::FilePath& cache_directory) {
  // TODO(crbug.com/295891): consider changing back to -imageWithContentsOfFile
  // instead of -imageWithData if both rdar://15747161 and the bug incorrectly
  // reporting the image as damaged https://stackoverflow.com/q/5081297/5353
  // are fixed.
  base::FilePath file_path =
      ImagePath(snapshot_id, image_type, image_scale, cache_directory);
  NSString* path = base::mac::FilePathToNSString(file_path);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  return [UIImage imageWithData:[NSData dataWithContentsOfFile:path]
                          scale:(image_type == IMAGE_TYPE_GREYSCALE
                                     ? 1.0
                                     : ScaleFromImageScale(image_scale))];
}

void WriteImageToDisk(UIImage* image, const base::FilePath& file_path) {
  if (!image)
    return;
  // CGImage should exist, otherwise UIImageJPEG(PNG)Representation returns nil.
  CHECK(image.CGImage);

  base::FilePath directory = file_path.DirName();
  if (!base::DirectoryExists(directory)) {
    bool success = base::CreateDirectory(directory);
    if (!success) {
      DLOG(ERROR) << "Error creating thumbnail directory "
                  << directory.AsUTF8Unsafe();
      return;
    }
  }

  NSString* path = base::mac::FilePathToNSString(file_path);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSData* data = UIImageJPEGRepresentation(image, kJPEGImageQuality);
  if (!data) {
    // Use UIImagePNGRepresentation instead when ImageJPEGRepresentation returns
    // nil. It happens when the underlying CGImageRef contains data in an
    // unsupported bitmap format.
    data = UIImagePNGRepresentation(image);
  }
  [data writeToFile:path atomically:YES];

  // Encrypt the snapshot file (mostly for Incognito, but can't hurt to
  // always do it).
  NSDictionary* attribute_dict = [NSDictionary
      dictionaryWithObject:NSFileProtectionCompleteUntilFirstUserAuthentication
                    forKey:NSFileProtectionKey];
  NSError* error = nil;
  BOOL success = [[NSFileManager defaultManager] setAttributes:attribute_dict
                                                  ofItemAtPath:path
                                                         error:&error];
  if (!success) {
    DLOG(ERROR) << "Error encrypting thumbnail file "
                << base::SysNSStringToUTF8([error description]);
  }
}

void ConvertAndSaveGreyImage(NSString* snapshot_id,
                             ImageScale image_scale,
                             UIImage* color_image,
                             const base::FilePath& cache_directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (!color_image) {
    color_image = ReadImageForSnapshotIDFromDisk(snapshot_id, IMAGE_TYPE_COLOR,
                                                 image_scale, cache_directory);
    if (!color_image)
      return;
  }
  UIImage* grey_image = GreyImage(color_image);
  base::FilePath image_path = ImagePath(snapshot_id, IMAGE_TYPE_GREYSCALE,
                                        image_scale, cache_directory);
  WriteImageToDisk(grey_image, image_path);
  base::apple::SetBackupExclusion(image_path);
}

void DeleteImageWithSnapshotID(const base::FilePath& cache_directory,
                               NSString* snapshot_id,
                               ImageScale snapshot_scale) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  for (const ImageType image_type : kImageTypes) {
    base::DeleteFile(
        ImagePath(snapshot_id, image_type, snapshot_scale, cache_directory));
  }
}

void RemoveAllImages(const base::FilePath& cache_directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (cache_directory.empty() || !base::DirectoryExists(cache_directory))
    return;

  if (!base::DeletePathRecursively(cache_directory)) {
    DLOG(ERROR) << "Error deleting snapshots storage. "
                << cache_directory.AsUTF8Unsafe();
  }
  if (!base::CreateDirectory(cache_directory)) {
    DLOG(ERROR) << "Error creating snapshot storage "
                << cache_directory.AsUTF8Unsafe();
  }
}

void PurgeCacheOlderThan(const base::FilePath& cache_directory,
                         const base::Time& threshold_date,
                         NSSet<NSString*>* keep_alive_snapshot_ids,
                         ImageScale snapshot_scale) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (!base::DirectoryExists(cache_directory))
    return;

  std::set<base::FilePath> files_to_keep;
  for (NSString* snapshot_id in keep_alive_snapshot_ids) {
    for (const ImageType image_type : kImageTypes) {
      files_to_keep.insert(
          ImagePath(snapshot_id, image_type, snapshot_scale, cache_directory));
    }
  }
  base::FileEnumerator enumerator(cache_directory, false,
                                  base::FileEnumerator::FILES);

  for (base::FilePath current_file = enumerator.Next(); !current_file.empty();
       current_file = enumerator.Next()) {
    if (current_file.Extension() != ".jpg")
      continue;
    if (base::Contains(files_to_keep, current_file))
      continue;
    base::FileEnumerator::FileInfo file_info = enumerator.GetInfo();
    if (file_info.GetLastModifiedTime() > threshold_date)
      continue;

    base::DeleteFile(current_file);
  }
}

void RenameSnapshots(const base::FilePath& cache_directory,
                     NSArray<NSString*>* old_ids,
                     NSArray<NSString*>* new_ids,
                     ImageScale snapshot_scale) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  DCHECK(base::DirectoryExists(cache_directory));
  DCHECK_EQ(old_ids.count, new_ids.count);

  const NSUInteger count = old_ids.count;
  for (NSUInteger index = 0; index < count; ++index) {
    for (const ImageType image_type : kImageTypes) {
      const base::FilePath old_image_path = ImagePath(
          old_ids[index], image_type, snapshot_scale, cache_directory);
      const base::FilePath new_image_path = ImagePath(
          new_ids[index], image_type, snapshot_scale, cache_directory);

      // Only migrate snapshots that are needed.
      if (!base::PathExists(old_image_path) ||
          base::PathExists(new_image_path)) {
        continue;
      }

      if (!base::Move(old_image_path, new_image_path)) {
        DLOG(ERROR) << "Error migrating file: " << old_image_path.AsUTF8Unsafe()
                    << " to: " << new_image_path.AsUTF8Unsafe();
      }
    }
  }
}

void CopyImageFile(const base::FilePath& old_image_path,
                   const base::FilePath& new_image_path) {
  // Only migrate files that are needed.
  if (!base::PathExists(old_image_path) || base::PathExists(new_image_path)) {
    return;
  }

  if (!base::CopyFile(old_image_path, new_image_path)) {
    DLOG(ERROR) << "Error copying file: " << old_image_path.AsUTF8Unsafe()
                << " to: " << new_image_path.AsUTF8Unsafe();
  }
}

void CreateCacheDirectory(const base::FilePath& cache_directory) {
  // This is a NO-OP if the directory already exists.
  if (!base::CreateDirectory(cache_directory)) {
    DLOG(ERROR) << "Error creating snapshot storage: "
                << cache_directory.AsUTF8Unsafe();
  }
}

UIImage* GreyImageFromCachedImage(const base::FilePath& cache_directory,
                                  NSString* snapshot_id,
                                  ImageScale snapshot_scale,
                                  UIImage* cached_image) {
  // If the image is not in the cache, load it from disk.
  UIImage* image = cached_image;
  if (!image) {
    image = ReadImageForSnapshotIDFromDisk(snapshot_id, IMAGE_TYPE_COLOR,
                                           snapshot_scale, cache_directory);
  }

  if (!image)
    return nil;

  return GreyImage(image);
}

}  // anonymous namespace

@implementation SnapshotCache {
  // Cache to hold color snapshots in memory. n.b. Color snapshots are not
  // kept in memory on tablets.
  SnapshotLRUCache* _lruCache;

  // Temporary dictionary to hold grey snapshots for tablet side swipe. This
  // will be nil before -createGreyCache is called and after -removeGreyCache
  // is called.
  NSMutableDictionary<NSString*, UIImage*>* _greyImageDictionary;

  // Snapshot ID of most recent pending grey snapshot request.
  NSString* _mostRecentGreySnapshotID;
  // Block used by pending request for a grey snapshot.
  void (^_mostRecentGreyBlock)(UIImage*);

  // Snapshot ID and corresponding UIImage for the snapshot that will likely
  // be requested to be saved to disk when the application is backgrounded.
  NSString* _backgroundingSnapshotID;
  UIImage* _backgroundingColorImage;

  // Scale for snapshot images. May be smaller than the screen scale in order
  // to save memory on some devices.
  ImageScale _snapshotsScale;

  // Directory where the thumbnails are saved.
  base::FilePath _cacheDirectory;

  // Task runner used to run tasks in the background. Will be invalidated when
  // -shutdown is invoked. Code should support this value to be null (generally
  // by not posting the task).
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Check that public API is called from the correct sequence.
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ((self = [super init])) {
    NSUInteger cacheSize = IsPinnedTabsEnabled()
                               ? kLRUCacheMaxCapacityForPinnedTabsEnabled
                               : kLRUCacheMaxCapacity;
    _lruCache = [[SnapshotLRUCache alloc] initWithCacheSize:cacheSize];
    _cacheDirectory = storagePath;
    _snapshotsScale = ImageScaleForDevice();

    _taskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

    // Must be called after task runner is created.
    [self createStorageIfNecessary];

    _observers = [SnapshotCacheObservers observers];

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
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleBecomeActive)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_taskRunner) << "-shutdown must be called before -dealloc";

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
  return ScaleFromImageScale(_snapshotsScale);
}

- (void)retrieveImageForSnapshotID:(NSString*)snapshotID
                          callback:(void (^)(UIImage*))callback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(snapshotID);
  DCHECK(callback);

  if (UIImage* image = [_lruCache objectForKey:snapshotID]) {
    callback(image);
    return;
  }

  if (!_taskRunner) {
    callback(nil);
    return;
  }

  __weak SnapshotLRUCache* weakLRUCache = _lruCache;
  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadImageForSnapshotIDFromDisk, snapshotID,
                     IMAGE_TYPE_COLOR, _snapshotsScale, _cacheDirectory),
      base::BindOnce(^(UIImage* image) {
        if (image)
          [weakLRUCache setObject:image forKey:snapshotID];
        callback(image);
      }));
}

- (void)setImage:(UIImage*)image withSnapshotID:(NSString*)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!image || !snapshotID || !_taskRunner)
    return;

  [_lruCache setObject:image forKey:snapshotID];

  // Each image in the cache has the same resolution and hence the same size.
  size_t imageSizes = CGImageGetBytesPerRow(image.CGImage) *
                      CGImageGetHeight(image.CGImage) * [_lruCache count];
  base::UmaHistogramMemoryKB("IOS.Snapshots.CacheSize", imageSizes / 1024);

  [self.observers snapshotCache:self didUpdateSnapshotForID:snapshotID];

  // Save the image to disk.
  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&WriteImageToDisk, image,
                                ImagePath(snapshotID, IMAGE_TYPE_COLOR,
                                          _snapshotsScale, _cacheDirectory)));
}

- (void)removeImageWithSnapshotID:(NSString*)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  [_lruCache removeObjectForKey:snapshotID];

  [self.observers snapshotCache:self didUpdateSnapshotForID:snapshotID];

  if (!_taskRunner)
    return;

  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&DeleteImageWithSnapshotID, _cacheDirectory,
                                snapshotID, _snapshotsScale));
}

- (void)removeAllImages {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  [_lruCache removeAllObjects];

  if (!_taskRunner)
    return;

  _taskRunner->PostTask(FROM_HERE,
                        base::BindOnce(&RemoveAllImages, _cacheDirectory));
}

- (base::FilePath)imagePathForSnapshotID:(NSString*)snapshotID {
  return ImagePath(snapshotID, IMAGE_TYPE_COLOR, _snapshotsScale,
                   _cacheDirectory);
}

- (base::FilePath)greyImagePathForSnapshotID:(NSString*)snapshotID {
  return ImagePath(snapshotID, IMAGE_TYPE_GREYSCALE, _snapshotsScale,
                   _cacheDirectory);
}

- (void)purgeCacheOlderThan:(const base::Time&)date
                    keeping:(NSSet*)liveSnapshotIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (!_taskRunner)
    return;

  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&PurgeCacheOlderThan, _cacheDirectory, date,
                                liveSnapshotIDs, _snapshotsScale));
}

- (void)renameSnapshotsWithIDs:(NSArray<NSString*>*)oldIDs
                         toIDs:(NSArray<NSString*>*)newIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner) {
    return;
  }

  DCHECK_EQ(oldIDs.count, newIDs.count);
  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&RenameSnapshots, _cacheDirectory, oldIDs,
                                newIDs, _snapshotsScale));
}

- (void)migrateImageWithSnapshotID:(NSString*)snapshotID
                   toSnapshotCache:(SnapshotCache*)destinationCache {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // Copy to the destination cache.
  if (UIImage* image = [_lruCache objectForKey:snapshotID]) {
    // Copy both on-disk and in-memory versions.
    [destinationCache setImage:image withSnapshotID:snapshotID];
    // Copy the grey scale version, if available.
    if (UIImage* greyImage = [_greyImageDictionary objectForKey:snapshotID]) {
      [destinationCache->_greyImageDictionary setObject:greyImage
                                                 forKey:snapshotID];
    }
  } else {
    // Only copy on-disk.
    if (_taskRunner) {
      _taskRunner->PostTask(
          FROM_HERE,
          base::BindOnce(&CopyImageFile,
                         [self imagePathForSnapshotID:snapshotID],
                         [destinationCache imagePathForSnapshotID:snapshotID]));
      _taskRunner->PostTask(
          FROM_HERE,
          base::BindOnce(
              &CopyImageFile, [self greyImagePathForSnapshotID:snapshotID],
              [destinationCache greyImagePathForSnapshotID:snapshotID]));
    }
  }

  // Remove the snapshot from this cache.
  [self removeImageWithSnapshotID:snapshotID];
}

- (void)willBeSavedGreyWhenBackgrounding:(NSString*)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!snapshotID)
    return;
  _backgroundingSnapshotID = [snapshotID copy];
  _backgroundingColorImage = [_lruCache objectForKey:snapshotID];
}

// Remove all but adjacent UIImages from `lruCache_`.
- (void)handleLowMemory {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSMutableDictionary<NSString*, UIImage*>* dictionary =
      [NSMutableDictionary dictionaryWithCapacity:2];
  for (NSString* snapshotID in self.pinnedSnapshotIDs) {
    UIImage* image = [_lruCache objectForKey:snapshotID];
    if (image)
      [dictionary setObject:image forKey:snapshotID];
  }
  [_lruCache removeAllObjects];
  for (NSString* snapshotID in self.pinnedSnapshotIDs) {
    [_lruCache setObject:[dictionary objectForKey:snapshotID]
                  forKey:snapshotID];
  }
}

// Remove all UIImages from `lruCache_`.
- (void)handleEnterBackground {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_lruCache removeAllObjects];
}

// Restore adjacent UIImages to `lruCache_`.
- (void)handleBecomeActive {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  for (NSString* snapshotID in self.pinnedSnapshotIDs) {
    [self retrieveImageForSnapshotID:snapshotID
                            callback:^(UIImage*){
                            }];
  }
}

// Save grey image to `greyImageDictionary_` and call into most recent
// `_mostRecentGreyBlock` if `_mostRecentGreySnapshotID` matches `snapshotID`.
- (void)saveGreyImage:(UIImage*)greyImage forSnapshotID:(NSString*)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (greyImage)
    [_greyImageDictionary setObject:greyImage forKey:snapshotID];
  if ([snapshotID isEqualToString:_mostRecentGreySnapshotID]) {
    _mostRecentGreyBlock(greyImage);
    [self clearGreySnapshotInfo];
  }
}

// Load uncached snapshot image and convert image to grey.
- (void)loadGreyImageAsync:(NSString*)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Don't call -retrieveImageForSnapshotID here because it caches the colored
  // image, which we don't need for the grey image cache. But if the image is
  // already in the cache, use it.
  UIImage* image = [_lruCache objectForKey:snapshotID];

  if (!_taskRunner)
    return;

  __weak SnapshotCache* weakSelf = self;
  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GreyImageFromCachedImage, _cacheDirectory, snapshotID,
                     _snapshotsScale, image),
      base::BindOnce(^(UIImage* greyImage) {
        [weakSelf saveGreyImage:greyImage forSnapshotID:snapshotID];
      }));
}

- (void)createGreyCache:(NSArray*)snapshotIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _greyImageDictionary =
      [NSMutableDictionary dictionaryWithCapacity:kGreyInitialCapacity];
  for (NSString* snapshotID in snapshotIDs)
    [self loadGreyImageAsync:snapshotID];
}

- (void)removeGreyCache {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _greyImageDictionary = nil;
  [self clearGreySnapshotInfo];
}

// Clear most recent caller information.
- (void)clearGreySnapshotInfo {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _mostRecentGreySnapshotID = nil;
  _mostRecentGreyBlock = nil;
}

- (void)greyImageForSnapshotID:(NSString*)snapshotID
                      callback:(void (^)(UIImage*))callback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(_greyImageDictionary);
  DCHECK(snapshotID);
  DCHECK(callback);

  if (UIImage* image = [_greyImageDictionary objectForKey:snapshotID]) {
    callback(image);
    [self clearGreySnapshotInfo];
  } else {
    _mostRecentGreySnapshotID = [snapshotID copy];
    _mostRecentGreyBlock = [callback copy];
  }
}

- (void)retrieveGreyImageForSnapshotID:(NSString*)snapshotID
                              callback:(void (^)(UIImage*))callback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(snapshotID);
  DCHECK(callback);

  if (_greyImageDictionary) {
    if (UIImage* image = [_greyImageDictionary objectForKey:snapshotID]) {
      callback(image);
      return;
    }
  }

  if (!_taskRunner) {
    callback(nil);
    return;
  }

  __weak SnapshotCache* weakSelf = self;
  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadImageForSnapshotIDFromDisk, snapshotID,
                     IMAGE_TYPE_GREYSCALE, _snapshotsScale, _cacheDirectory),
      base::BindOnce(^(UIImage* image) {
        if (image) {
          callback(image);
          return;
        }
        [weakSelf retrieveImageForSnapshotID:snapshotID
                                    callback:^(UIImage* snapshotImage) {
                                      if (snapshotImage) {
                                        snapshotImage =
                                            GreyImage(snapshotImage);
                                      }
                                      callback(snapshotImage);
                                    }];
      }));
}

- (void)saveGreyInBackgroundForSnapshotID:(NSString*)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!snapshotID)
    return;

  // The color image may still be in memory.  Verify the snapshotID matches.
  if (_backgroundingColorImage) {
    if (![_backgroundingSnapshotID isEqualToString:snapshotID]) {
      _backgroundingSnapshotID = nil;
      _backgroundingColorImage = nil;
    }
  }

  if (!_taskRunner)
    return;

  _taskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(&ConvertAndSaveGreyImage, snapshotID, _snapshotsScale,
                     _backgroundingColorImage, _cacheDirectory));
}

- (void)addObserver:(id<SnapshotCacheObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<SnapshotCacheObserver>)observer {
  [self.observers removeObserver:observer];
}

- (void)shutdown {
  _taskRunner = nullptr;
}

#pragma mark - Private methods

- (void)createStorageIfNecessary {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner)
    return;

  _taskRunner->PostTask(FROM_HERE,
                        base::BindOnce(CreateCacheDirectory, _cacheDirectory));
}

@end

@implementation SnapshotCache (TestingAdditions)

- (BOOL)hasImageInMemory:(NSString*)snapshotID {
  return [_lruCache objectForKey:snapshotID] != nil;
}

- (BOOL)hasGreyImageInMemory:(NSString*)snapshotID {
  return [_greyImageDictionary objectForKey:snapshotID] != nil;
}

- (NSUInteger)lruCacheMaxSize {
  return [_lruCache maxCacheSize];
}

@end
