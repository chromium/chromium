// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/fake_user_uploaded_image_manager.h"

#import <UIKit/UIKey.h>

#import "base/strings/strcat.h"
#import "base/task/sequenced_task_runner.h"
#import "base/uuid.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

FakeUserUploadedImageManager::FakeUserUploadedImageManager(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : UserUploadedImageManager(base::FilePath(), task_runner),
      task_runner_(task_runner) {}

FakeUserUploadedImageManager::~FakeUserUploadedImageManager() = default;

base::FilePath FakeUserUploadedImageManager::StoreUserUploadedImage(
    UIImage* image) {
  // Generate UUID-based filename.
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  base::FilePath image_relative_file_path =
      base::FilePath(base::StrCat({uuid.AsLowercaseString(), ".jpg"}));

  images_[image_relative_file_path] = image;
  return image_relative_file_path;
}

void FakeUserUploadedImageManager::StoreUserUploadedImage(
    UIImage* image,
    base::OnceCallback<void(base::FilePath)> callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), StoreUserUploadedImage(image)));
}

UIImage* FakeUserUploadedImageManager::LoadUserUploadedImage(
    base::FilePath relative_image_file_path) {
  auto found_image = images_.find(relative_image_file_path);
  return (found_image != images_.end()) ? found_image->second : nil;
}

void FakeUserUploadedImageManager::LoadUserUploadedImage(
    base::FilePath relative_image_file_path,
    UserUploadImageCallback callback) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](UIImage* image, UserUploadImageCallback cb) {
                       UserUploadedImageError error =
                           image ? UserUploadedImageError::kNone
                                 : UserUploadedImageError::kFailedToReadFile;
                       std::move(cb).Run(image, error);
                     },
                     LoadUserUploadedImage(relative_image_file_path),
                     std::move(callback)));
}

void FakeUserUploadedImageManager::DeleteUserUploadedImageSynchronously(
    base::FilePath relative_image_file_path) {
  images_.erase(relative_image_file_path);
}
void FakeUserUploadedImageManager::DeleteUserUploadedImage(
    base::FilePath relative_image_file_path,
    base::OnceClosure completion) {
  DeleteUserUploadedImageSynchronously(relative_image_file_path);
  task_runner_->PostTask(FROM_HERE, std::move(completion));
}

void FakeUserUploadedImageManager::DeleteUnusedImagesSynchronously(
    std::set<base::FilePath> relative_file_paths_in_use) {
  std::erase_if(images_,
                [&relative_file_paths_in_use](
                    const std::pair<const base::FilePath, UIImage*>& item) {
                  return !relative_file_paths_in_use.contains(item.first);
                });
}

void FakeUserUploadedImageManager::DeleteUnusedImages(
    std::set<base::FilePath> relative_file_paths_in_use,
    base::OnceClosure completion) {
  DeleteUnusedImagesSynchronously(relative_file_paths_in_use);
  task_runner_->PostTask(FROM_HERE, std::move(completion));
}
