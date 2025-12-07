// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"

#import <UIKit/UIKit.h>

#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// The directory name that the UserUploadedImageManager uses to store images.
const char image_directory[] = "BackgroundImages";

// Helper function to generate a test image.
UIImage* GenerateTestImage(CGSize size) {
  UIGraphicsImageRendererFormat* format =
      [[UIGraphicsImageRendererFormat alloc] init];
  format.scale = 1;
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:size format:format];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    [UIColor.redColor setFill];
    [context fillRect:CGRectMake(0, 0, size.width, size.height)];
  }];
}

// Returns a callback that captures its arguments and stores them into
// `outputs`.
template <typename... Types>
base::OnceCallback<void(Types...)> CaptureArgs(Types&... outputs) {
  return base::BindOnce(
      [](Types&... outputs, Types... args) { ((outputs = args), ...); },
      std::ref(outputs)...);
}

}  // namespace

class UserUploadedImageManagerTest : public PlatformTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    image_manager_ = std::make_unique<UserUploadedImageManager>(
        data_dir_.GetPath(), base::SequencedTaskRunner::GetCurrentDefault());
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir data_dir_;
  std::unique_ptr<UserUploadedImageManager> image_manager_;
};

// Tests that storing an image stores a file correctly.
TEST_F(UserUploadedImageManagerTest, StoreImage) {
  UIImage* test_image = GenerateTestImage(CGSizeMake(1, 1));

  base::RunLoop run_loop;
  base::FilePath relative_image_file_path;
  image_manager_->StoreUserUploadedImage(
      test_image,
      CaptureArgs(relative_image_file_path).Then(run_loop.QuitClosure()));

  run_loop.Run();

  base::FilePath full_image_file_path = data_dir_.GetPath()
                                            .Append(image_directory)
                                            .Append(relative_image_file_path);
  EXPECT_TRUE(base::PathExists(full_image_file_path));
}

// Tests that loading an image loads the image that was stored.
TEST_F(UserUploadedImageManagerTest, LoadImage) {
  UIImage* test_image = GenerateTestImage(CGSizeMake(2, 2));

  base::RunLoop store_run_loop;
  base::FilePath relative_image_file_path;
  image_manager_->StoreUserUploadedImage(
      test_image,
      CaptureArgs(relative_image_file_path).Then(store_run_loop.QuitClosure()));

  store_run_loop.Run();

  base::RunLoop load_run_loop;
  UIImage* loaded_image;
  UserUploadedImageError error;
  image_manager_->LoadUserUploadedImage(
      relative_image_file_path,
      CaptureArgs(loaded_image, error).Then(load_run_loop.QuitClosure()));

  load_run_loop.Run();

  ASSERT_NSNE(nil, loaded_image);
  EXPECT_EQ(UserUploadedImageError::kNone, error);

  // The image is compressed and converted before being stored, so the bytes may
  // not be identical. Compare sizes to check similarity.
  EXPECT_TRUE(CGSizeEqualToSize(test_image.size, loaded_image.size));
}

// Tests that loading a non-existent image returns nil.
TEST_F(UserUploadedImageManagerTest, LoadNonexistentImage) {
  base::RunLoop run_loop;
  UIImage* loaded_image = GenerateTestImage(CGSizeMake(3, 3));
  UserUploadedImageError error = UserUploadedImageError::kNone;
  image_manager_->LoadUserUploadedImage(
      base::FilePath("nonexistent_image.png"),
      CaptureArgs(loaded_image, error).Then(run_loop.QuitClosure()));

  run_loop.Run();

  ASSERT_NSEQ(nil, loaded_image);
  EXPECT_EQ(UserUploadedImageError::kFailedToReadFile, error);
}

// Tests that images can be deleted.
TEST_F(UserUploadedImageManagerTest, DeleteImage) {
  UIImage* test_image = GenerateTestImage(CGSizeMake(4, 4));

  base::RunLoop store_run_loop;
  base::FilePath relative_image_file_path;
  image_manager_->StoreUserUploadedImage(
      test_image,
      CaptureArgs(relative_image_file_path).Then(store_run_loop.QuitClosure()));

  store_run_loop.Run();

  base::FilePath full_image_file_path = data_dir_.GetPath()
                                            .Append(image_directory)
                                            .Append(relative_image_file_path);
  ASSERT_TRUE(base::PathExists(full_image_file_path));

  base::RunLoop delete_run_loop;
  image_manager_->DeleteUserUploadedImage(relative_image_file_path,
                                          delete_run_loop.QuitClosure());

  delete_run_loop.Run();

  ASSERT_FALSE(base::PathExists(full_image_file_path));
}

// Tests that deleting unused images keeps in-use images and deletes those not
// in-use.
TEST_F(UserUploadedImageManagerTest, DeleteUnusedImages) {
  UIImage* test_image1 = GenerateTestImage(CGSizeMake(5, 5));
  UIImage* test_image2 = GenerateTestImage(CGSizeMake(6, 6));

  base::RunLoop store_run_loop;
  base::FilePath relative_image_file_path1;
  base::FilePath relative_image_file_path2;
  image_manager_->StoreUserUploadedImage(
      test_image1, CaptureArgs(relative_image_file_path1));
  image_manager_->StoreUserUploadedImage(
      test_image2, CaptureArgs(relative_image_file_path2)
                       .Then(store_run_loop.QuitClosure()));

  store_run_loop.Run();

  base::FilePath full_image_file_path1 = data_dir_.GetPath()
                                             .Append(image_directory)
                                             .Append(relative_image_file_path1);
  ASSERT_TRUE(base::PathExists(full_image_file_path1));
  base::FilePath full_image_file_path2 = data_dir_.GetPath()
                                             .Append(image_directory)
                                             .Append(relative_image_file_path2);
  ASSERT_TRUE(base::PathExists(full_image_file_path2));

  // Say only image 1 is still in use. This should cause image 2 to be deleted.
  base::RunLoop delete_run_loop;
  std::set<base::FilePath> in_use_image_paths = {relative_image_file_path1};
  image_manager_->DeleteUnusedImages(in_use_image_paths,
                                     delete_run_loop.QuitClosure());

  delete_run_loop.Run();

  ASSERT_TRUE(base::PathExists(full_image_file_path1));
  ASSERT_FALSE(base::PathExists(full_image_file_path2));
}
