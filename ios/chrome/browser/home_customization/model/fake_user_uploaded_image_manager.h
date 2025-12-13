// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_FAKE_USER_UPLOADED_IMAGE_MANAGER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_FAKE_USER_UPLOADED_IMAGE_MANAGER_H_

#import <map>

#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"

// A fake implementation of UserUploadedImageManager for use in tests. It holds
// its images in memory instead of reading and writing to disk.
class FakeUserUploadedImageManager : public UserUploadedImageManager {
 public:
  explicit FakeUserUploadedImageManager(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~FakeUserUploadedImageManager() override;

  // UserUploadedImageManager methods done synchronously.
  base::FilePath StoreUserUploadedImage(UIImage* image);
  UIImage* LoadUserUploadedImage(base::FilePath relative_image_file_path);
  void DeleteUserUploadedImageSynchronously(
      base::FilePath relative_image_file_path);
  void DeleteUnusedImagesSynchronously(
      std::set<base::FilePath> relative_file_paths_in_use);

  // UserUploadedImageManager:
  void StoreUserUploadedImage(
      UIImage* image,
      base::OnceCallback<void(base::FilePath)> callback) override;
  void LoadUserUploadedImage(base::FilePath relative_image_file_path,
                             UserUploadImageCallback callback) override;
  void DeleteUserUploadedImage(
      base::FilePath relative_image_file_path,
      base::OnceClosure completion = base::DoNothing()) override;
  void DeleteUnusedImages(
      std::set<base::FilePath> relative_file_paths_in_use,
      base::OnceClosure completion = base::DoNothing()) override;

 private:
  // Task runner for the background tasks.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::map<base::FilePath, UIImage*> images_;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_FAKE_USER_UPLOADED_IMAGE_MANAGER_H_
