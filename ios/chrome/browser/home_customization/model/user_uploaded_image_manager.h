// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_USER_UPLOADED_IMAGE_MANAGER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_USER_UPLOADED_IMAGE_MANAGER_H_

#import <set>

#import "base/files/file_path.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

@class UIImage;

// Service to handle storing and loading user-uploaded images.
class UserUploadedImageManager : public KeyedService {
 public:
  explicit UserUploadedImageManager(
      const base::FilePath& base_user_file_path,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  UserUploadedImageManager(const UserUploadedImageManager&) = delete;
  UserUploadedImageManager& operator=(const UserUploadedImageManager&) = delete;
  ~UserUploadedImageManager() override;

  // Stores the provided image. Also generates a filename for the image and
  // calls the callback with that filename or an empty path if image storing
  // failed. This path is relative, so should only be loaded with
  // `LoadUserUploadedImage`.
  virtual void StoreUserUploadedImage(
      UIImage* image,
      base::OnceCallback<void(base::FilePath)> callback);

  using UserUploadImageCallback =
      base::OnceCallback<void(UIImage*, UserUploadedImageError)>;
  // Loads an image previously stored at the provided relative file path.
  virtual void LoadUserUploadedImage(base::FilePath relative_image_file_path,
                                     UserUploadImageCallback callback);

  // Deletes an image previously stored at the provided relative file path.
  virtual void DeleteUserUploadedImage(
      base::FilePath relative_image_file_path,
      base::OnceClosure completion = base::DoNothing());

  // Deletes all images from the managed directory that aren't currently in use.
  virtual void DeleteUnusedImages(
      std::set<base::FilePath> relative_file_paths_in_use,
      base::OnceClosure completion = base::DoNothing());

  // Returns the full, absolute path to an image file.
  base::FilePath GetFullImagePath(
      const base::FilePath& relative_image_file_path) const;

 private:
  // File path to store images at.
  const base::FilePath storage_directory_path_;

  // Task runner to run background image storage tasks on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_USER_UPLOADED_IMAGE_MANAGER_H_
