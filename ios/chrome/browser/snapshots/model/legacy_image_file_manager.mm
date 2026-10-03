// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/legacy_image_file_manager.h"

#import "base/apple/backup_util.h"
#import "base/apple/foundation_util.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/sequence_checker.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_scale.h"

namespace {

enum ImageType {
  IMAGE_TYPE_COLOR,
  IMAGE_TYPE_GREYSCALE,
};

const ImageType kImageTypes[] = {
    IMAGE_TYPE_COLOR,
    IMAGE_TYPE_GREYSCALE,
};

// Default JPEG quality (no compression).
const CGFloat kJPEGImageQualityDefault = 1.0;

// Compressed JPEG quality. Provides visually lossless quality while reducing
// file size by ~3-5x compared to 1.0.
const CGFloat kJPEGImageQualityCompressed = 0.97;

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
    case kImageScale1X:
      return "";
    case kImageScale2X:
      return "@2x";
  }
}

// Returns the path of the image for `snapshot_id`, in `directory`,
// of type `image_type` and scale `image_scale`.
base::FilePath ImagePath(SnapshotID snapshot_id,
                         ImageType image_type,
                         ImageScale image_scale,
                         const base::FilePath& directory) {
  const std::string filename = base::StringPrintf(
      "%08u%s%s.jpg", snapshot_id.identifier(), SuffixForImageType(image_type),
      SuffixForImageScale(image_scale));
  return directory.Append(filename);
}

// Returns the path of the image for `snapshot_id`, in `directory`,
// of type `image_type` and scale `image_scale`.
base::FilePath LegacyImagePath(NSString* snapshot_id,
                               ImageType image_type,
                               ImageScale image_scale,
                               const base::FilePath& directory) {
  const std::string filename = base::StringPrintf(
      "%s%s%s.jpg", base::SysNSStringToUTF8(snapshot_id).c_str(),
      SuffixForImageType(image_type), SuffixForImageScale(image_scale));
  return directory.Append(filename);
}

// Creates a directory that images are stored.
void CreateStorageDirectory(const base::FilePath& directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  // This is a NO-OP if the directory already exists.
  if (!base::CreateDirectory(directory)) {
    const base::File::Error error = base::File::GetLastFileError();
    DLOG(ERROR) << "Error creating snapshot storage: "
                << directory.AsUTF8Unsafe() << ": "
                << base::File::ErrorToString(error);
  }
}

// Helper function to read an image from disk.
UIImage* ReadImageForSnapshotIDFromDisk(SnapshotID snapshot_id,
                                        ImageScale image_scale,
                                        const base::FilePath& directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  base::FilePath file_path =
      ImagePath(snapshot_id, IMAGE_TYPE_COLOR, image_scale, directory);
  NSString* path = base::apple::FilePathToNSString(file_path);
  // Downsampled images are stored at half the device scale, so read
  // them back at the same reduced scale to preserve point dimensions.
  CGFloat device_scale = [SnapshotImageScale floatImageScaleForDevice];
  CGFloat read_scale =
      IsSnapshotDownsampleImageEnabled() ? (device_scale / 2.0) : device_scale;
  return [UIImage imageWithData:[NSData dataWithContentsOfFile:path]
                          scale:read_scale];
}

// Downsamples `image` when enabled, synchronously attempts to write it to
// `file_path`, and returns the image used for encoding. The return value does
// not indicate whether the write succeeded.
UIImage* DownsampleAndWriteImageToDisk(UIImage* image,
                                       const base::FilePath& file_path) {
  if (!image) {
    return nil;
  }
  if (!image.CGImage) {
    // It's possible that CGImage doesn't exist for the chrome:// pages when
    // it's an official build.
    // TODO(crbug.com/40284759): Investigate why it happens and how to solve it.
    return image;
  }

  UIImage* image_to_write = image;
  if (IsSnapshotDownsampleImageEnabled()) {
    @autoreleasepool {
      CGSize target_size = image.size;
      UIGraphicsImageRendererFormat* format =
          [[UIGraphicsImageRendererFormat alloc] init];
      format.scale = image.scale / 2.0;
      UIGraphicsImageRenderer* renderer =
          [[UIGraphicsImageRenderer alloc] initWithSize:target_size
                                                 format:format];
      image_to_write = [renderer
          imageWithActions:^(UIGraphicsImageRendererContext* context) {
            [image drawInRect:CGRectMake(0, 0, target_size.width,
                                         target_size.height)];
          }];
    }
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  base::FilePath directory = file_path.DirName();
  if (!base::DirectoryExists(directory)) {
    bool success = base::CreateDirectory(directory);
    if (!success) {
      DLOG(ERROR) << "Error creating thumbnail directory "
                  << directory.AsUTF8Unsafe();
      return image_to_write;
    }
  }

  NSString* path = base::apple::FilePathToNSString(file_path);
  const CGFloat quality =
      base::FeatureList::IsEnabled(kSnapshotCompressedJPEGQuality)
          ? kJPEGImageQualityCompressed
          : kJPEGImageQualityDefault;
  NSData* data = UIImageJPEGRepresentation(image_to_write, quality);
  if (!data) {
    // Use UIImagePNGRepresentation instead when ImageJPEGRepresentation returns
    // nil. It happens when the underlying CGImageRef contains data in an
    // unsupported bitmap format.
    data = UIImagePNGRepresentation(image_to_write);
  }

  base::UmaHistogramMemoryKB(
      "IOS.Snapshots.DowngradedQualityImageMemoryFootprint",
      [data length] / 1024);
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
  return image_to_write;
}

// Helper function to delete an image from disk.
void DeleteImageWithSnapshotID(const base::FilePath& directory,
                               SnapshotID snapshot_id,
                               ImageScale snapshot_scale) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  for (const ImageType image_type : kImageTypes) {
    base::DeleteFile(
        ImagePath(snapshot_id, image_type, snapshot_scale, directory));
  }
}

void RemoveAllImages(const base::FilePath& directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (!base::DirectoryExists(directory)) {
    return;
  }

  if (!base::DeletePathRecursively(directory)) {
    DLOG(ERROR) << "Error deleting snapshots storage. "
                << directory.AsUTF8Unsafe();
  }
  if (!base::CreateDirectory(directory)) {
    DLOG(ERROR) << "Error creating snapshot storage "
                << directory.AsUTF8Unsafe();
  }
}

// Helper function to delete images created before `threshold_date` from disk.
void PurgeImagesOlderThan(
    const base::FilePath& directory,
    base::Time threshold_date,
    const std::vector<SnapshotID>& keep_alive_snapshot_ids,
    ImageScale snapshot_scale) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (!base::DirectoryExists(directory)) {
    return;
  }

  std::set<base::FilePath> files_to_keep;
  for (SnapshotID snapshot_id : keep_alive_snapshot_ids) {
    for (const ImageType image_type : kImageTypes) {
      files_to_keep.insert(
          ImagePath(snapshot_id, image_type, snapshot_scale, directory));
    }
  }
  base::FileEnumerator enumerator(directory, false,
                                  base::FileEnumerator::FILES);

  for (base::FilePath current_file = enumerator.Next(); !current_file.empty();
       current_file = enumerator.Next()) {
    if (current_file.Extension() != ".jpg") {
      continue;
    }
    if (files_to_keep.contains(current_file)) {
      continue;
    }
    base::FileEnumerator::FileInfo file_info = enumerator.GetInfo();
    if (file_info.GetLastModifiedTime() > threshold_date) {
      continue;
    }

    base::DeleteFile(current_file);
  }
}

// Helper function to copy an image from `old_image_path` to `new_image_path`.
void CopyImageFile(const base::FilePath& old_image_path,
                   const base::FilePath& new_image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  // Only migrate files that are needed.
  if (!base::PathExists(old_image_path) || base::PathExists(new_image_path)) {
    return;
  }

  if (!base::CopyFile(old_image_path, new_image_path)) {
    DLOG(ERROR) << "Error copying file: " << old_image_path.AsUTF8Unsafe()
                << " to: " << new_image_path.AsUTF8Unsafe();
  }
}

}  // anonymous namespace

@implementation LegacyImageFileManager {
  // Directory where the thumbnails are saved.
  base::FilePath _storageDirectory;

  // Scale for snapshot images. May be smaller than the screen scale in order
  // to save memory on some devices.
  ImageScale _snapshotsScale;

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
    _storageDirectory = storagePath;
    _snapshotsScale = [SnapshotImageScale imageScaleForDevice];

    _taskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

    _taskRunner->PostTask(
        FROM_HERE, base::BindOnce(CreateStorageDirectory, _storageDirectory));
  }
  return self;
}

- (void)readImageWithSnapshotID:(SnapshotID)snapshotID
                     completion:(ImageReadCompletionBlock)completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK(snapshotID.valid());
  DCHECK(completion);
  if (!_taskRunner) {
    std::move(completion).Run(nil);
    return;
  }
  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadImageForSnapshotIDFromDisk, snapshotID,
                     _snapshotsScale, _storageDirectory),
      std::move(completion));
}

- (void)writeImage:(UIImage*)image withSnapshotID:(SnapshotID)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner) {
    return;
  }
  _taskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&DownsampleAndWriteImageToDisk), image,
                     ImagePath(snapshotID, IMAGE_TYPE_COLOR, _snapshotsScale,
                               _storageDirectory)));
}

- (void)writeImage:(UIImage*)image
    withSnapshotID:(SnapshotID)snapshotID
        completion:(ImageWriteCompletionBlock)completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner) {
    // No disk write is performed after shutdown. Return the original image so
    // callers waiting for the completion are not left pending.
    std::move(completion).Run(image);
    return;
  }
  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownsampleAndWriteImageToDisk, image,
                     ImagePath(snapshotID, IMAGE_TYPE_COLOR, _snapshotsScale,
                               _storageDirectory)),
      std::move(completion));
}

- (void)removeImageWithSnapshotID:(SnapshotID)snapshotID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner) {
    return;
  }
  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&DeleteImageWithSnapshotID, _storageDirectory,
                                snapshotID, _snapshotsScale));
}

- (void)removeAllImages {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner) {
    return;
  }
  _taskRunner->PostTask(FROM_HERE,
                        base::BindOnce(&RemoveAllImages, _storageDirectory));
}

- (void)purgeImagesOlderThan:(base::Time)date
                     keeping:(const std::vector<SnapshotID>&)liveSnapshotIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner) {
    return;
  }
  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&PurgeImagesOlderThan, _storageDirectory, date,
                                liveSnapshotIDs, _snapshotsScale));
}

- (void)copyImage:(const base::FilePath&)oldPath
        toNewPath:(const base::FilePath&)newPath {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_taskRunner) {
    return;
  }
  _taskRunner->PostTask(FROM_HERE,
                        base::BindOnce(&CopyImageFile, oldPath, newPath));
}

- (base::FilePath)imagePathForSnapshotID:(SnapshotID)snapshotID {
  return ImagePath(snapshotID, IMAGE_TYPE_COLOR, _snapshotsScale,
                   _storageDirectory);
}

- (base::FilePath)legacyImagePathForSnapshotID:(NSString*)snapshotID {
  return LegacyImagePath(snapshotID, IMAGE_TYPE_COLOR, _snapshotsScale,
                         _storageDirectory);
}

- (void)shutdown {
  _taskRunner = nullptr;
}

- (void)dealloc {
  DCHECK(!_taskRunner) << "-shutdown must be called before -dealloc";
}

@end
