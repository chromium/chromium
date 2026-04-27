// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"
#import "base/task/sequenced_task_runner.h"
#import "base/uuid.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

const char image_directory[] = "BackgroundImages";
const char image_filename_prefix[] = "background_image_";

// A struct that holds the result of a user-uploaded image load operation.
struct LoadImageResult {
  UIImage* image;
  CGSize original_size;
  UserUploadedImageError error;
};

// Compresses and saves `image` to the provided `directory_path`. Also generates
// a UUID-based filename for `image` and returns the full save path (or an empty
// path if saving failed).
base::FilePath SaveImageToDirectory(const base::FilePath& directory_path,
                                    UIImage* image) {
  // Create directory if it doesn't exist.
  if (!base::CreateDirectory(directory_path)) {
    LOG(ERROR) << "Failed to create directory: " << directory_path.value();
    base::UmaHistogramEnumeration(
        "IOS.HomeCustomization.Background.UserUploaded.Error",
        UserUploadedImageError::kFailedToCreateDirectory);
    return base::FilePath();
  }

  // Convert image to JPEG.
  NSData* image_data = UIImageJPEGRepresentation(image, 0.9);
  if (!image_data) {
    base::UmaHistogramEnumeration(
        "IOS.HomeCustomization.Background.UserUploaded.Error",
        UserUploadedImageError::kFailedToConvertToJPEG);
    return base::FilePath();
  }

  // Generate UUID-based filename.
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  base::FilePath image_relative_file_path = base::FilePath(
      base::StrCat({image_filename_prefix, uuid.AsLowercaseString(), ".jpg"}));
  base::FilePath file_path = directory_path.Append(image_relative_file_path);

  const std::string_view data_string(
      reinterpret_cast<const char*>([image_data bytes]), [image_data length]);
  if (!base::WriteFile(file_path, data_string)) {
    LOG(ERROR) << "Failed to write file: " << file_path.value();
    base::UmaHistogramEnumeration(
        "IOS.HomeCustomization.Background.UserUploaded.Error",
        UserUploadedImageError::kFailedToWriteFile);
    return base::FilePath();
  }

  base::UmaHistogramEnumeration(
      "IOS.HomeCustomization.Background.UserUploaded.Error",
      UserUploadedImageError::kNone);
  return image_relative_file_path;
}

// Loads the image at the given path. If kNTPBackgroundDownsampleImage is
// enabled, downsamples to `target_point_size` at 2x scale.
// Otherwise, loads at full resolution. Also returns the original image size.
LoadImageResult LoadImageAtPath(const base::FilePath& path,
                                CGSize target_point_size) {
  NSURL* image_url =
      [NSURL fileURLWithPath:base::apple::FilePathToNSString(path)];

  // Get original image size before downsampling.
  CGSize original_size = ImageSizeFromURL(image_url);

  UIImage* image;
  if (base::FeatureList::IsEnabled(kNTPBackgroundDownsampleImage)) {
    // Use 2x scale — all devices supporting iOS 17+ are at least 2x, and
    // capping at 2x is sufficient for a background image.
    image = DownsampledImageFromURL(image_url, target_point_size, 2.0);
  } else {
    NSData* data = [NSData dataWithContentsOfURL:image_url];
    image = [UIImage imageWithData:data];
  }
  if (!image) {
    return {nil, CGSizeZero, UserUploadedImageError::kFailedToReadFile};
  }

  base::UmaHistogramMemoryKB(
      "IOS.HomeCustomization.Background.UserUploaded.ImageMemoryFootprint",
      MemoryFootprintForImage(image));
  return {image, original_size, UserUploadedImageError::kNone};
}

// Deletes any unused image files that exist in `directory_path` but not in
// `relative_file_paths_in_use`.
void DeleteUnusedImageFilePaths(
    const base::FilePath& directory_path,
    std::set<base::FilePath> relative_file_paths_in_use) {
  base::FilePath::StringType file_pattern =
      base::StrCat({image_filename_prefix, "*", ".jpg"});
  base::FileEnumerator e(directory_path, false, base::FileEnumerator::FILES,
                         file_pattern);
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    base::FilePath base_name = name.BaseName();
    if (relative_file_paths_in_use.contains(base_name)) {
      continue;
    }
    base::DeleteFile(name);
  }
}
}  // namespace

UserUploadedImageManager::UserUploadedImageManager(
    const base::FilePath& base_user_file_path,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : storage_directory_path_(base_user_file_path.AppendASCII(image_directory)),
      task_runner_(task_runner) {}

UserUploadedImageManager::~UserUploadedImageManager() {}

void UserUploadedImageManager::StoreUserUploadedImage(
    UIImage* image,
    base::OnceCallback<void(base::FilePath)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SaveImageToDirectory, storage_directory_path_, image),
      std::move(callback));
}

void UserUploadedImageManager::LoadUserUploadedImage(
    base::FilePath relative_image_file_path,
    CGSize target_point_size,
    UserUploadImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath full_file_path =
      storage_directory_path_.Append(relative_image_file_path);

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadImageAtPath, full_file_path, target_point_size),
      base::BindOnce(
          [](UserUploadImageCallback original_callback,
             LoadImageResult result) {
            std::move(original_callback)
                .Run(result.image, result.original_size, result.error);
          },
          std::move(callback)));
}

void UserUploadedImageManager::DeleteUserUploadedImage(
    base::FilePath relative_image_file_path,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath file_path =
      storage_directory_path_.Append(relative_image_file_path);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), file_path)
          .Then(std::move(completion)));
}

void UserUploadedImageManager::DeleteUnusedImages(
    std::set<base::FilePath> relative_file_paths_in_use,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_runner_->PostTask(FROM_HERE, base::BindOnce(&DeleteUnusedImageFilePaths,
                                                   storage_directory_path_,
                                                   relative_file_paths_in_use)
                                        .Then(std::move(completion)));
}

base::FilePath UserUploadedImageManager::GetFullImagePath(
    const base::FilePath& relative_image_file_path) const {
  return storage_directory_path_.Append(relative_image_file_path);
}
