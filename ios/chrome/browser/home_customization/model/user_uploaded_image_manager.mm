// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/callback_forward.h"
#import "base/logging.h"
#import "base/strings/strcat.h"
#import "base/task/sequenced_task_runner.h"
#import "base/uuid.h"

namespace {

const char image_directory[] = "BackgroundImages";
const char image_filename_prefix[] = "background_image_";

// Compresses and saves `image` to the provided `directory_path`. Also generates
// a UUID-based filename for `image` and returns the full save path (or an empty
// path if saving failed).
base::FilePath SaveImageToDirectory(const base::FilePath& directory_path,
                                    UIImage* image) {
  // Create directory if it doesn't exist.
  if (!base::CreateDirectory(directory_path)) {
    LOG(ERROR) << "Failed to create directory: " << directory_path.value();
    return base::FilePath();
  }

  // Convert image to JPEG.
  NSData* image_data = UIImageJPEGRepresentation(image, 0.9);
  if (!image_data) {
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
    return base::FilePath();
  }

  return image_relative_file_path;
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

UIImage* UserUploadedImageManager::LoadUserUploadedImage(
    base::FilePath relative_image_file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath full_file_path =
      storage_directory_path_.Append(relative_image_file_path);
  NSURL* imageURL =
      [NSURL fileURLWithPath:base::apple::FilePathToNSString(full_file_path)];

  // Load the image from disk.
  NSData* imageData = [NSData dataWithContentsOfURL:imageURL];
  if (!imageData) {
    return nil;
  }

  UIImage* image = [UIImage imageWithData:imageData];
  if (!image) {
    return nil;
  }

  return image;
}
