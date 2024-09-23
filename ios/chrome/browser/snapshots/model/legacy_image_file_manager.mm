// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/legacy_image_file_manager.h"

#import "base/apple/backup_util.h"
#import "base/apple/foundation_util.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/logging.h"
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

const CGFloat kJPEGImageQuality = 1.0;  // Highest quality. No compression.

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
void CreateStorageDirectory(const base::FilePath& directory,
                            const base::FilePath& legacy_directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  // This is a NO-OP if the directory already exists.
  if (!base::CreateDirectory(directory)) {
    const base::File::Error error = base::File::GetLastFileError();
    DLOG(ERROR) << "Error creating snapshot storage: "
                << directory.AsUTF8Unsafe() << ": "
                << base::File::ErrorToString(error);
    return;
  }

  if (!base::DirectoryExists(legacy_directory)) {
    return;
  }

  // If `legacy_directory` exists and is a directory, move its content to
  // `directory` and then delete the directory. As this function is
  // used to move snapshot file which are not stored recursively, limit
  // the enumeration to files and do not perform a recursive enumeration.
  base::FileEnumerator iter(legacy_directory, /*recursive=*/false,
                            base::FileEnumerator::FILES);

  for (base::FilePath item = iter.Next(); !item.empty(); item = iter.Next()) {
    base::FilePath to_path = directory;
    legacy_directory.AppendRelativePath(item, &to_path);
    base::Move(item, to_path);
  }

  // Delete the `legacy_directory` once the existing files have been moved.
  base::DeletePathRecursively(legacy_directory);
}

// Helper function to read an image from disk.
UIImage* ReadImageForSnapshotIDFromDisk(SnapshotID snapshot_id,
                                        ImageScale image_scale,
                                        const base::FilePath& directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  // TODO(crbug.com/41056111): consider changing back to
  // -imageWithContentsOfFile instead of -imageWithData if both rdar://15747161
  // and the bug incorrectly reporting the image as damaged
  // https://stackoverflow.com/q/5081297/5353 are fixed.
  base::FilePath file_path =
      ImagePath(snapshot_id, IMAGE_TYPE_COLOR, image_scale, directory);
  NSString* path = base::apple::FilePathToNSString(file_path);
  return [UIImage imageWithData:[NSData dataWithContentsOfFile:path]
                          scale:[SnapshotImageScale floatImageScaleForDevice]];
}

// Helper function to write an image to disk.
void WriteImageToDisk(UIImage* image, const base::FilePath& file_path) {
  if (!image) {
    return;
  }
  if (!image.CGImage) {
    // It's possible that CGImage doesn't exist for the chrome:// pages when
    // it's an official build.
    // TODO(crbug.com/40284759): Investigate why it happens and how to solve it.
    return;
  }
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

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
    const base::Time& threshold_date,
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

// Helper function to rename images from `old_ids` to `new_ids`.
void RenameSnapshots(const base::FilePath& directory,
                     NSArray<NSString*>* old_ids,
                     const std::vector<SnapshotID>& new_ids,
                     ImageScale snapshot_scale) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  DCHECK(base::DirectoryExists(directory));
  DCHECK_EQ(old_ids.count, new_ids.size());

  const NSUInteger count = old_ids.count;
  for (NSUInteger index = 0; index < count; ++index) {
    for (const ImageType image_type : kImageTypes) {
      const base::FilePath old_image_path = LegacyImagePath(
          old_ids[index], image_type, snapshot_scale, directory);
      const base::FilePath new_image_path =
          ImagePath(new_ids[index], image_type, snapshot_scale, directory);

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

// Frees up disk by deleting all grey snapshots if they exist in `directory`
// because grey snapshots are not stored anymore when
// `kGreySnapshotOptimization` feature is enabled.
// TODO(crbug.com/40279302): This function should be removed in a few milestones
// after `kGreySnapshotOptimization` feature is enabled by default.
void DeleteAllGreyImages(const base::FilePath& directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (!base::DirectoryExists(directory)) {
    return;
  }

  base::FileEnumerator iter(directory, /*recursive=*/false,
                            base::FileEnumerator::FILES);

  for (base::FilePath item = iter.Next(); !item.empty(); item = iter.Next()) {
    if (item.BaseName().value().find(
            SuffixForImageType(IMAGE_TYPE_GREYSCALE)) != std::string::npos) {
      base::DeleteFile(item);
    }
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

- (instancetype)initWithStoragePath:(const base::FilePath&)storagePath
                         legacyPath:(const base::FilePath&)legacyPath {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ((self = [super init])) {
    _storageDirectory = storagePath;
    _snapshotsScale = [SnapshotImageScale imageScaleForDevice];

    _taskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

    _taskRunner->PostTask(
        FROM_HERE,
        base::BindOnce(CreateStorageDirectory, _storageDirectory, legacyPath));

    // TODO(crbug.com/40279302): Delete this logic after a few milestones.
    _taskRunner->PostTask(
        FROM_HERE, base::BindOnce(DeleteAllGreyImages, _storageDirectory));
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
      FROM_HERE, base::BindOnce(&WriteImageToDisk, image,
                                ImagePath(snapshotID, IMAGE_TYPE_COLOR,
                                          _snapshotsScale, _storageDirectory)));
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

- (void)renameSnapshotsWithIDs:(NSArray<NSString*>*)oldIDs
                         toIDs:(const std::vector<SnapshotID>&)newIDs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK_EQ(oldIDs.count, newIDs.size());
  if (!_taskRunner) {
    return;
  }
  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&RenameSnapshots, _storageDirectory, oldIDs,
                                newIDs, _snapshotsScale));
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
