// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_storage.h"
#import "ios/chrome/browser/snapshots/snapshot_storage_internal.h"

#import <UIKit/UIKit.h>

#import <map>
#import <set>

#import "base/apple/backup_util.h"
#import "base/apple/foundation_util.h"
#import "base/base_paths.h"
#import "base/containers/contains.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/path_service.h"
#import "base/sequence_checker.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/snapshot_id.h"
#import "ios/chrome/browser/snapshots/snapshot_lru_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_storage_observer.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ui/base/device_form_factor.h"

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
// List of observers to be notified of changes to the snapshot cache.
@property(nonatomic, strong) SnapshotStorageObservers* observers;
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
    IMAGE_TYPE_COLOR,
    IMAGE_TYPE_GREYSCALE,
};

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

// Returns the suffix to append to image filename for `image_type`.
const char* SuffixForImageType(ImageType image_type) {
  switch (image_type) {
    case IMAGE_TYPE_COLOR:
      return "";
    case IMAGE_TYPE_GREYSCALE:
      return "Grey";
  }
}

// Returns the suffix to append to image filename for `image_scale`.
const char* SuffixForImageScale(ImageScale image_scale) {
  switch (image_scale) {
    case IMAGE_SCALE_1X:
      return "";
    case IMAGE_SCALE_2X:
      return "@2x";
    case IMAGE_SCALE_3X:
      return "@3x";
  }
}

// Returns the path of the image for `snapshot_id`, in `cache_directory`,
// of type `image_type` and scale `image_scale`.
base::FilePath ImagePath(SnapshotID snapshot_id,
                         ImageType image_type,
                         ImageScale image_scale,
                         const base::FilePath& cache_directory) {
  const std::string filename = base::StringPrintf(
      "%08u%s%s.jpg", snapshot_id.identifier(), SuffixForImageType(image_type),
      SuffixForImageScale(image_scale));
  return cache_directory.Append(filename);
}

// Returns the path of the image for `snapshot_id`, in `cache_directory`,
// of type `image_type` and scale `image_scale`.
base::FilePath LegacyImagePath(NSString* snapshot_id,
                               ImageType image_type,
                               ImageScale image_scale,
                               const base::FilePath& cache_directory) {
  const std::string filename = base::StringPrintf(
      "%s%s%s.jpg", base::SysNSStringToUTF8(snapshot_id).c_str(),
      SuffixForImageType(image_type), SuffixForImageScale(image_scale));
  return cache_directory.Append(filename);
}

ImageScale ImageScaleForDevice() {
  // On handset, the color snapshot is used for the stack view, so the scale of
  // the snapshot images should match the scale of the device.
  // On tablet, the color snapshot is only used to generate the grey snapshot,
  // which does not have to be high quality, so use scale of 1.0 on all tablets.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return IMAGE_SCALE_1X;
  }

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

UIImage* ReadImageForSnapshotIDFromDisk(SnapshotID snapshot_id,
                                        ImageType image_type,
                                        ImageScale image_scale,
                                        const base::FilePath& cache_directory) {
  // TODO(crbug.com/295891): consider changing back to -imageWithContentsOfFile
  // instead of -imageWithData if both rdar://15747161 and the bug incorrectly
  // reporting the image as damaged https://stackoverflow.com/q/5081297/5353
  // are fixed.
  base::FilePath file_path =
      ImagePath(snapshot_id, image_type, image_scale, cache_directory);
  NSString* path = base::apple::FilePathToNSString(file_path);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  return [UIImage imageWithData:[NSData dataWithContentsOfFile:path]
                          scale:(image_type == IMAGE_TYPE_GREYSCALE
                                     ? 1.0
                                     : ScaleFromImageScale(image_scale))];
}

void WriteImageToDisk(UIImage* image, const base::FilePath& file_path) {
  if (!image) {
    return;
  }
  if (!image.CGImage) {
    // It's possible that CGImage doesn't exist for the chrome:// pages when
    // it's an official build.
    // TODO(crbug.com/1490496): Investigate why it happens and how to solve it.
    return;
  }

  base::FilePath directory = file_path.DirName();
  if (!base::DirectoryExists(directory)) {
    bool success = base::CreateDirectory(directory);
    if (!success) {
      DLOG(ERROR) << "Error creating thumbnail directory "
                  << directory.AsUTF8Unsafe();
      return;
    }
  }

  NSString* path = base::apple::FilePathToNSString(file_path);
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

void ConvertAndSaveGreyImage(SnapshotID snapshot_id,
                             ImageScale image_scale,
                             UIImage* color_image,
                             const base::FilePath& cache_directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (!color_image) {
    color_image = ReadImageForSnapshotIDFromDisk(snapshot_id, IMAGE_TYPE_COLOR,
                                                 image_scale, cache_directory);
    if (!color_image) {
      return;
    }
  }
  UIImage* grey_image = GreyImage(color_image);
  base::FilePath image_path = ImagePath(snapshot_id, IMAGE_TYPE_GREYSCALE,
                                        image_scale, cache_directory);
  WriteImageToDisk(grey_image, image_path);
  base::apple::SetBackupExclusion(image_path);
}

void DeleteImageWithSnapshotID(const base::FilePath& cache_directory,
                               SnapshotID snapshot_id,
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

  if (cache_directory.empty() || !base::DirectoryExists(cache_directory)) {
    return;
  }

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
                         const std::vector<SnapshotID>& keep_alive_snapshot_ids,
                         ImageScale snapshot_scale) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (!base::DirectoryExists(cache_directory)) {
    return;
  }

  std::set<base::FilePath> files_to_keep;
  for (SnapshotID snapshot_id : keep_alive_snapshot_ids) {
    for (const ImageType image_type : kImageTypes) {
      files_to_keep.insert(
          ImagePath(snapshot_id, image_type, snapshot_scale, cache_directory));
    }
  }
  base::FileEnumerator enumerator(cache_directory, false,
                                  base::FileEnumerator::FILES);

  for (base::FilePath current_file = enumerator.Next(); !current_file.empty();
       current_file = enumerator.Next()) {
    if (current_file.Extension() != ".jpg") {
      continue;
    }
    if (base::Contains(files_to_keep, current_file)) {
      continue;
    }
    base::FileEnumerator::FileInfo file_info = enumerator.GetInfo();
    if (file_info.GetLastModifiedTime() > threshold_date) {
      continue;
    }

    base::DeleteFile(current_file);
  }
}

void RenameSnapshots(const base::FilePath& cache_directory,
                     NSArray<NSString*>* old_ids,
                     const std::vector<SnapshotID>& new_ids,
                     ImageScale snapshot_scale) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  DCHECK(base::DirectoryExists(cache_directory));
  DCHECK_EQ(old_ids.count, new_ids.size());

  const NSUInteger count = old_ids.count;
  for (NSUInteger index = 0; index < count; ++index) {
    for (const ImageType image_type : kImageTypes) {
      const base::FilePath old_image_path = LegacyImagePath(
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

void CreateCacheDirectory(const base::FilePath& cache_directory,
                          const base::FilePath& legacy_directory) {
  // This is a NO-OP if the directory already exists.
  if (!base::CreateDirectory(cache_directory)) {
    const base::File::Error error = base::File::GetLastFileError();
    DLOG(ERROR) << "Error creating snapshot storage: "
                << cache_directory.AsUTF8Unsafe() << ": "
                << base::File::ErrorToString(error);
    return;
  }

  if (legacy_directory.empty() || !base::DirectoryExists(legacy_directory)) {
    return;
  }

  // If `legacy_directory` exists and is a directory, move its content to
  // `cache_directory` and then delete the directory. As this function is
  // used to move snapshot file which are not stored recursively, limit
  // the enumeration to files and do not perform a recursive enumeration.
  base::FileEnumerator iter(legacy_directory, /*recursive*/ false,
                            base::FileEnumerator::FILES);

  for (base::FilePath item = iter.Next(); !item.empty(); item = iter.Next()) {
    base::FilePath to_path = cache_directory;
    legacy_directory.AppendRelativePath(item, &to_path);
    base::Move(item, to_path);
  }

  // Delete the `legacy_directory` once the existing files have been moved.
  base::DeletePathRecursively(legacy_directory);
}

UIImage* GreyImageFromCachedImage(const base::FilePath& cache_directory,
                                  SnapshotID snapshot_id,
                                  ImageScale snapshot_scale,
                                  UIImage* cached_image) {
  // If the image is not in the cache, load it from disk.
  UIImage* image = cached_image;
  if (!image) {
    image = ReadImageForSnapshotIDFromDisk(snapshot_id, IMAGE_TYPE_COLOR,
                                           snapshot_scale, cache_directory);
  }

  if (!image) {
    return nil;
  }

  return GreyImage(image);
}

}  // anonymous namespace

@implementation SnapshotStorage {
  // Cache to hold color snapshots in memory. n.b. Color snapshots are not
  // kept in memory on tablets.
  SnapshotLRUCache<UIImage*>* _lruCache;

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

- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath
                         legacyPath:(const base::FilePath&)legacyPath {
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

    _taskRunner->PostTask(
        FROM_HERE,
        base::BindOnce(CreateCacheDirectory, _cacheDirectory, legacyPath));

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

- (void)retrieveImageForSnapshotID:(SnapshotID)snapshotID
                          callback:(void (^)(UIImage*))callback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(snapshotID.valid());
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
        if (image) {
          [weakLRUCache setObject:image forKey:snapshotID];
        }
        callback(image);
      }));
}

- (void)setImage:(UIImage*)image withSnapshotID:(SnapshotID)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!image || !snapshotID.valid() || !_taskRunner) {
    return;
  }

  [_lruCache setObject:image forKey:snapshotID];

  // Each image in the cache has the same resolution and hence the same size.
  size_t imageSizes = CGImageGetBytesPerRow(image.CGImage) *
                      CGImageGetHeight(image.CGImage) * [_lruCache count];
  base::UmaHistogramMemoryKB("IOS.Snapshots.CacheSize", imageSizes / 1024);

  [self.observers snapshotStorage:self didUpdateSnapshotForID:snapshotID];

  // Save the image to disk.
  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&WriteImageToDisk, image,
                                ImagePath(snapshotID, IMAGE_TYPE_COLOR,
                                          _snapshotsScale, _cacheDirectory)));
}

- (void)removeImageWithSnapshotID:(SnapshotID)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  [_lruCache removeObjectForKey:snapshotID];

  [self.observers snapshotStorage:self didUpdateSnapshotForID:snapshotID];

  if (!_taskRunner) {
    return;
  }

  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&DeleteImageWithSnapshotID, _cacheDirectory,
                                snapshotID, _snapshotsScale));
}

- (void)removeAllImages {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  [_lruCache removeAllObjects];

  if (!_taskRunner) {
    return;
  }

  _taskRunner->PostTask(FROM_HERE,
                        base::BindOnce(&RemoveAllImages, _cacheDirectory));
}

- (base::FilePath)imagePathForSnapshotID:(SnapshotID)snapshotID {
  return ImagePath(snapshotID, IMAGE_TYPE_COLOR, _snapshotsScale,
                   _cacheDirectory);
}

- (base::FilePath)legacyImagePathForSnapshotID:(NSString*)snapshotID {
  return LegacyImagePath(snapshotID, IMAGE_TYPE_COLOR, _snapshotsScale,
                         _cacheDirectory);
}

- (base::FilePath)greyImagePathForSnapshotID:(SnapshotID)snapshotID {
  return ImagePath(snapshotID, IMAGE_TYPE_GREYSCALE, _snapshotsScale,
                   _cacheDirectory);
}

- (void)purgeCacheOlderThan:(base::Time)date
                    keeping:(const std::vector<SnapshotID>&)liveSnapshotIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (!_taskRunner) {
    return;
  }

  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&PurgeCacheOlderThan, _cacheDirectory, date,
                                liveSnapshotIDs, _snapshotsScale));
}

- (void)renameSnapshotsWithIDs:(NSArray<NSString*>*)oldIDs
                         toIDs:(const std::vector<SnapshotID>&)newIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner) {
    return;
  }

  DCHECK_EQ(oldIDs.count, newIDs.size());
  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&RenameSnapshots, _cacheDirectory, oldIDs,
                                newIDs, _snapshotsScale));
}

- (void)migrateImageWithSnapshotID:(SnapshotID)snapshotID
                 toSnapshotStorage:(SnapshotStorage*)destinationStorage {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // Copy to the destination storage.
  if (UIImage* image = [_lruCache objectForKey:snapshotID]) {
    // Copy both on-disk and in-memory versions.
    [destinationStorage setImage:image withSnapshotID:snapshotID];
    // Copy the grey scale version, if available.
    auto iterator = _greyImageDictionary.find(snapshotID);
    if (iterator != _greyImageDictionary.end()) {
      destinationStorage->_greyImageDictionary.insert(
          std::make_pair(snapshotID, iterator->second));
    }
  } else {
    // Only copy on-disk.
    if (_taskRunner) {
      _taskRunner->PostTask(
          FROM_HERE,
          base::BindOnce(
              &CopyImageFile, [self imagePathForSnapshotID:snapshotID],
              [destinationStorage imagePathForSnapshotID:snapshotID]));
      _taskRunner->PostTask(
          FROM_HERE,
          base::BindOnce(
              &CopyImageFile, [self greyImagePathForSnapshotID:snapshotID],
              [destinationStorage greyImagePathForSnapshotID:snapshotID]));
    }
  }

  // Remove the snapshot from this cache.
  [self removeImageWithSnapshotID:snapshotID];
}

- (void)willBeSavedGreyWhenBackgrounding:(SnapshotID)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!snapshotID.valid()) {
    return;
  }
  _backgroundingSnapshotID = snapshotID;
  _backgroundingColorImage = [_lruCache objectForKey:snapshotID];
}

// Remove all UIImages from `lruCache_`.
- (void)handleLowMemory {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_lruCache removeAllObjects];
}

// Remove all UIImages from `lruCache_`.
- (void)handleEnterBackground {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_lruCache removeAllObjects];
}

// Save grey image to `greyImageDictionary_` and call into most recent
// `_mostRecentGreyBlock` if `_mostRecentGreySnapshotID` matches `snapshotID`.
- (void)saveGreyImage:(UIImage*)greyImage forSnapshotID:(SnapshotID)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Don't call -retrieveImageForSnapshotID here because it caches the colored
  // image, which we don't need for the grey image cache. But if the image is
  // already in the cache, use it.
  UIImage* image = [_lruCache objectForKey:snapshotID];

  if (!_taskRunner) {
    return;
  }

  __weak SnapshotStorage* weakSelf = self;
  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GreyImageFromCachedImage, _cacheDirectory, snapshotID,
                     _snapshotsScale, image),
      base::BindOnce(^(UIImage* greyImage) {
        [weakSelf saveGreyImage:greyImage forSnapshotID:snapshotID];
      }));
}

- (void)createGreyCache:(const std::vector<SnapshotID>&)snapshotIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _greyImageDictionary.clear();
  for (SnapshotID snapshotID : snapshotIDs) {
    [self loadGreyImageAsync:snapshotID];
  }
}

- (void)removeGreyCache {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _greyImageDictionary.clear();
  [self clearGreySnapshotInfo];
}

// Clear most recent caller information.
- (void)clearGreySnapshotInfo {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _mostRecentGreySnapshotID = SnapshotID();
  _mostRecentGreyBlock = nil;
}

- (void)retrieveGreyImageForSnapshotID:(SnapshotID)snapshotID
                              callback:(void (^)(UIImage*))callback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(snapshotID.valid());
  DCHECK(callback);

  auto iterator = _greyImageDictionary.find(snapshotID);
  if (iterator != _greyImageDictionary.end()) {
    callback(iterator->second);
    return;
  }

  if (!_taskRunner) {
    callback(nil);
    return;
  }

  __weak SnapshotStorage* weakSelf = self;
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

- (void)saveGreyInBackgroundForSnapshotID:(SnapshotID)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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

  if (!_taskRunner) {
    return;
  }

  _taskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(&ConvertAndSaveGreyImage, snapshotID, _snapshotsScale,
                     _backgroundingColorImage, _cacheDirectory));
}

- (void)addObserver:(id<SnapshotStorageObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<SnapshotStorageObserver>)observer {
  [self.observers removeObserver:observer];
}

- (void)shutdown {
  _taskRunner = nullptr;
}

@end

@implementation SnapshotStorage (TestingAdditions)

- (void)greyImageForSnapshotID:(SnapshotID)snapshotID
                      callback:(void (^)(UIImage*))callback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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

- (BOOL)hasImageInMemory:(SnapshotID)snapshotID {
  return [_lruCache objectForKey:snapshotID] != nil;
}

- (BOOL)hasGreyImageInMemory:(SnapshotID)snapshotID {
  return base::Contains(_greyImageDictionary, snapshotID);
}

- (NSUInteger)lruCacheMaxSize {
  return [_lruCache maxCacheSize];
}

@end
