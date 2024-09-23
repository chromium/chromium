// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/functional/callback_forward.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/legacy_image_file_manager.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_scale.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

const NSUInteger kSnapshotCount = 10;
const NSUInteger kSnapshotPixelSize = 8;

class LegacyImageFileManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(CreateImageFileManager());
  }

  void TearDown() override {
    ClearAllImages();
    [image_file_manager_ shutdown];
    image_file_manager_ = nil;
    PlatformTest::TearDown();
  }

  // Build an array of snapshot IDs and an array of UIImages filled with
  // random colors.
  [[nodiscard]] bool CreateImageFileManager() {
    DCHECK(!image_file_manager_);
    if (!scoped_temp_directory_.CreateUniqueTempDir()) {
      return false;
    }

    image_file_manager_ = [[LegacyImageFileManager alloc]
        initWithStoragePath:scoped_temp_directory_.GetPath()
                 legacyPath:base::FilePath()];

    CGFloat scale = [SnapshotImageScale floatImageScaleForDevice];

    srand(1);

    for (NSUInteger i = 0; i < kSnapshotCount; ++i) {
      test_images_.insert(std::make_pair(
          SnapshotID(SessionID::NewUnique().id()), GenerateRandomImage(scale)));
    }

    return true;
  }

  LegacyImageFileManager* GetImageFileManager() {
    DCHECK(image_file_manager_);
    return image_file_manager_;
  }

  // Generates an image of `scale`, filled with a random color.
  UIImage* GenerateRandomImage(CGFloat scale) {
    CGSize size = CGSizeMake(kSnapshotPixelSize, kSnapshotPixelSize);
    UIGraphicsImageRendererFormat* format =
        [UIGraphicsImageRendererFormat preferredFormat];
    format.scale = scale;
    format.opaque = NO;

    UIGraphicsImageRenderer* renderer =
        [[UIGraphicsImageRenderer alloc] initWithSize:size format:format];

    return [renderer
        imageWithActions:^(UIGraphicsImageRendererContext* UIContext) {
          CGContextRef context = UIContext.CGContext;
          CGFloat r = rand() / CGFloat(RAND_MAX);
          CGFloat g = rand() / CGFloat(RAND_MAX);
          CGFloat b = rand() / CGFloat(RAND_MAX);
          CGContextSetRGBStrokeColor(context, r, g, b, 1.0);
          CGContextSetRGBFillColor(context, r, g, b, 1.0);
          CGContextFillRect(context, CGRectMake(0.0, 0.0, kSnapshotPixelSize,
                                                kSnapshotPixelSize));
        }];
  }

  // Flushes all the runloops internally used by the snapshot storage. This is
  // done by asking to retrieve a non-existent image from disk and blocking
  // until the callback is invoked.
  void FlushRunLoops() {
    if (!image_file_manager_) {
      return;
    }

    base::RunLoop run_loop;
    [image_file_manager_
        readImageWithSnapshotID:SnapshotID(SessionID::NewUnique().id())
                     completion:base::BindOnce(base::IgnoreArgs<UIImage*>(
                                    run_loop.QuitClosure()))];
    run_loop.Run();
  }

  // This function removes all snapshots stored in disk.
  void ClearAllImages() {
    if (!image_file_manager_) {
      return;
    }

    [image_file_manager_ removeAllImages];
    FlushRunLoops();

    __block BOOL foundImage = NO;
    __block NSUInteger numCallbacks = 0;
    for (auto [snapshot_id, _] : test_images_) {
      const base::FilePath path =
          [image_file_manager_ imagePathForSnapshotID:snapshot_id];

      // Checks that the snapshot is not on disk.
      EXPECT_FALSE(base::PathExists(path));

      // Check that the snapshot is not in the dictionary.
      [image_file_manager_
          readImageWithSnapshotID:snapshot_id
                       completion:base::BindOnce(^(UIImage* image) {
                         ++numCallbacks;
                         if (image) {
                           foundImage = YES;
                         }
                       })];
    }

    // Expect that all the callbacks ran and that none retrieved an image.
    FlushRunLoops();

    EXPECT_EQ(test_images_.size(), numCallbacks);
    EXPECT_FALSE(foundImage);
  }

  // Guesses the order of the color channels in the image.
  // Supports RGB, BGR, RGBA, BGRA, ARGB, ABGR.
  // Returns the position of each channel between 0 and 3.
  void ComputeColorComponents(CGImageRef cgImage,
                              int* red,
                              int* green,
                              int* blue) {
    CGBitmapInfo bitmapInfo = CGImageGetBitmapInfo(cgImage);
    CGImageAlphaInfo alphaInfo = CGImageGetAlphaInfo(cgImage);
    int byteOrder = bitmapInfo & kCGBitmapByteOrderMask;

    *red = 0;
    *green = 1;
    *blue = 2;

    if (alphaInfo == kCGImageAlphaLast ||
        alphaInfo == kCGImageAlphaPremultipliedLast ||
        alphaInfo == kCGImageAlphaNoneSkipLast) {
      *red = 1;
      *green = 2;
      *blue = 3;
    }

    if (byteOrder != kCGBitmapByteOrder32Host) {
      int lastChannel = (CGImageGetBitsPerPixel(cgImage) == 24) ? 2 : 3;
      *red = lastChannel - *red;
      *green = lastChannel - *green;
      *blue = lastChannel - *blue;
    }
  }

  web::WebTaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_directory_;
  LegacyImageFileManager* image_file_manager_;
  std::map<SnapshotID, UIImage*> test_images_;
};

// Tests that the color of all snapshots in the storage reloaded from disk.
TEST_F(LegacyImageFileManagerTest, CheckImageColors) {
  LegacyImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  // Put all images to disk.
  for (auto [snapshot_id, image] : test_images_) {
    [file_manager writeImage:image withSnapshotID:snapshot_id];
  }
  FlushRunLoops();

  for (auto [snapshot_id, reference_image] : test_images_) {
    // Check that images are on the disk.
    const base::FilePath path =
        [file_manager imagePathForSnapshotID:snapshot_id];
    EXPECT_TRUE(base::PathExists(path));

    // Check image colors by comparing the first pixel against the reference
    // image.
    UIImage* image =
        [UIImage imageWithContentsOfFile:base::SysUTF8ToNSString(path.value())];
    CGImageRef cgImage = [image CGImage];
    ASSERT_TRUE(cgImage != nullptr);

    base::apple::ScopedCFTypeRef<CFDataRef> pixelData(
        CGDataProviderCopyData(CGImageGetDataProvider(cgImage)));
    const char* pixels =
        reinterpret_cast<const char*>(CFDataGetBytePtr(pixelData.get()));
    EXPECT_TRUE(pixels);

    CGImageRef referenceCgImage = [reference_image CGImage];
    base::apple::ScopedCFTypeRef<CFDataRef> referenceData(
        CGDataProviderCopyData(CGImageGetDataProvider(referenceCgImage)));
    const char* referencePixels =
        reinterpret_cast<const char*>(CFDataGetBytePtr(referenceData.get()));
    EXPECT_TRUE(referencePixels);

    if (pixels != nil && referencePixels != nil) {
      // Color components may not be in the same order,
      // because of writing to disk and reloading.
      int red, green, blue;
      ComputeColorComponents(cgImage, &red, &green, &blue);

      int referenceRed, referenceGreen, referenceBlue;
      ComputeColorComponents(referenceCgImage, &referenceRed, &referenceGreen,
                             &referenceBlue);

      // Colors may not be exactly the same (compression or rounding errors)
      // thus a small difference is allowed.
      EXPECT_NEAR(referencePixels[referenceRed], pixels[red], 1);
      EXPECT_NEAR(referencePixels[referenceGreen], pixels[green], 1);
      EXPECT_NEAR(referencePixels[referenceBlue], pixels[blue], 1);
    }
  }
}

// Tests that old images are deleted.
TEST_F(LegacyImageFileManagerTest, PurgeImagesOlderThan) {
  LegacyImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  // Put all images in the storage.
  for (auto [snapshot_id, image] : test_images_) {
    [file_manager writeImage:image withSnapshotID:snapshot_id];
  }

  ASSERT_FALSE(test_images_.empty());
  std::vector<SnapshotID> liveSnapshotIDs = {test_images_.begin()->first};

  // Purge the storage.
  [file_manager purgeImagesOlderThan:(base::Time::Now() - base::Hours(1))
                             keeping:liveSnapshotIDs];
  FlushRunLoops();

  // Check that nothing has been deleted.
  for (auto [snapshot_id, _] : test_images_) {
    // Check that images are on the disk.
    const base::FilePath path =
        [file_manager imagePathForSnapshotID:snapshot_id];
    EXPECT_TRUE(base::PathExists(path));
  }

  // Purge the storage.
  [file_manager purgeImagesOlderThan:base::Time::Now() keeping:liveSnapshotIDs];
  FlushRunLoops();

  // Check that the file have been deleted.
  for (auto [snapshot_id, _] : test_images_) {
    // Check that images are on the disk.
    const base::FilePath path =
        [file_manager imagePathForSnapshotID:snapshot_id];
    if (snapshot_id == *liveSnapshotIDs.begin()) {
      EXPECT_TRUE(base::PathExists(path));
    } else {
      EXPECT_FALSE(base::PathExists(path));
    }
  }
}

// Tests that migration code correctly rename the specified files and leave
// the other files untouched.
TEST_F(LegacyImageFileManagerTest, RenameSnapshots) {
  LegacyImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  // This snapshot will be renamed.
  NSString* image1_id = [[NSUUID UUID] UUIDString];
  base::FilePath image1_path =
      [file_manager legacyImagePathForSnapshotID:image1_id];
  ASSERT_TRUE(base::WriteFile(image1_path, "image1"));

  // This snapshot will not be renamed.
  NSString* image2_id = [[NSUUID UUID] UUIDString];
  base::FilePath image2_path =
      [file_manager legacyImagePathForSnapshotID:image2_id];
  ASSERT_TRUE(base::WriteFile(image2_path, "image2"));

  SnapshotID new_id = SnapshotID(SessionID::NewUnique().id());
  [file_manager renameSnapshotsWithIDs:@[ image1_id ] toIDs:{new_id}];
  FlushRunLoops();

  // image1 should have been moved.
  EXPECT_FALSE(base::PathExists(image1_path));
  EXPECT_TRUE(base::PathExists([file_manager imagePathForSnapshotID:new_id]));

  // image2 should not have moved.
  EXPECT_TRUE(base::PathExists(image2_path));
}

// Tests that image size and scale are preserved when writing and reading
// from disk.
TEST_F(LegacyImageFileManagerTest, SizeAndScalePreservation) {
  LegacyImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  // Create an image with the expected snapshot scale.
  CGFloat scale = [SnapshotImageScale floatImageScaleForDevice];
  UIImage* image = GenerateRandomImage(scale);

  // Add the image to the storage and ensure the file is written to disk.
  const SnapshotID kSnapshotID(SessionID::NewUnique().id());
  [file_manager writeImage:image withSnapshotID:kSnapshotID];
  FlushRunLoops();

  // Retrive the image and have the callback verify the size and scale.
  __block BOOL callbackComplete = NO;
  [file_manager
      readImageWithSnapshotID:kSnapshotID
                   completion:base::BindOnce(^(UIImage* imageFromDisk) {
                     EXPECT_EQ(image.size.width, imageFromDisk.size.width);
                     EXPECT_EQ(image.size.height, imageFromDisk.size.height);
                     EXPECT_EQ(image.scale, imageFromDisk.scale);
                     callbackComplete = YES;
                   })];
  FlushRunLoops();
  EXPECT_TRUE(callbackComplete);
}

// Tests that retina-scale images are deleted properly.
TEST_F(LegacyImageFileManagerTest, DeleteRetinaImages) {
  LegacyImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  if ([SnapshotImageScale floatImageScaleForDevice] != 2.0) {
    return;
  }

  // Create an image with retina scale.
  UIImage* image = GenerateRandomImage(2.0);

  // Add the image and ensure the file is written to disk.
  const SnapshotID kSnapshotID(SessionID::NewUnique().id());
  [file_manager writeImage:image withSnapshotID:kSnapshotID];
  FlushRunLoops();

  // Verify the file was written with @2x in the file name.
  base::FilePath retinaFile = [file_manager imagePathForSnapshotID:kSnapshotID];
  EXPECT_TRUE(base::PathExists(retinaFile));

  // Delete the image and ensure the file is removed.
  [file_manager removeImageWithSnapshotID:kSnapshotID];
  FlushRunLoops();

  EXPECT_FALSE(base::PathExists(retinaFile));
}

// Tests that an image is immediately deleted when calling
// `-removeImageWithSnapshotID:`.
TEST_F(LegacyImageFileManagerTest, ImageDeleted) {
  LegacyImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  UIImage* image = GenerateRandomImage(0);
  const SnapshotID kSnapshotID(SessionID::NewUnique().id());
  [file_manager writeImage:image withSnapshotID:kSnapshotID];

  base::FilePath image_path = [file_manager imagePathForSnapshotID:kSnapshotID];

  // Remove the image and ensure the file is removed.
  [file_manager removeImageWithSnapshotID:kSnapshotID];
  FlushRunLoops();

  EXPECT_FALSE(base::PathExists(image_path));
}

// Tests that all images are deleted when calling `-removeAllImages`.
TEST_F(LegacyImageFileManagerTest, AllImagesDeleted) {
  LegacyImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  UIImage* image = GenerateRandomImage(0);
  const SnapshotID kSnapshotID1(SessionID::NewUnique().id());
  const SnapshotID kSnapshotID2(SessionID::NewUnique().id());
  [file_manager writeImage:image withSnapshotID:kSnapshotID1];
  [file_manager writeImage:image withSnapshotID:kSnapshotID2];
  base::FilePath image_1_path =
      [file_manager imagePathForSnapshotID:kSnapshotID1];
  base::FilePath image_2_path =
      [file_manager imagePathForSnapshotID:kSnapshotID2];

  // Remove all images and ensure the files are removed.
  [file_manager removeAllImages];
  FlushRunLoops();

  EXPECT_FALSE(base::PathExists(image_1_path));
  EXPECT_FALSE(base::PathExists(image_2_path));
}

// This is a duplicated test class of LegacyImageFileManagerTest to test
// ImageFileManager. We can't use value-parameterized tests because some
// public APIs are different from LegacyImageFileManager.
class ImageFileManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(CreateImageFileManager());
  }

  void TearDown() override {
    ClearAllImages();
    image_file_manager_ = nil;
    PlatformTest::TearDown();
  }

  // Build an array of snapshot IDs and an array of UIImages filled with
  // random colors.
  [[nodiscard]] bool CreateImageFileManager() {
    DCHECK(!image_file_manager_);
    if (!scoped_temp_directory_.CreateUniqueTempDir()) {
      return false;
    }
    if (!scoped_temp_directory_for_legacy_path_.CreateUniqueTempDir()) {
      return false;
    }

    NSURL* storage_url =
        base::apple::FilePathToNSURL(scoped_temp_directory_.GetPath());
    NSURL* legacy_url = base::apple::FilePathToNSURL(
        scoped_temp_directory_for_legacy_path_.GetPath());
    image_file_manager_ =
        [[ImageFileManager alloc] initWithStorageDirectoryUrl:storage_url
                                           legacyDirectoryUrl:legacy_url];
    // Make sure that the storage directory is ready.
    FlushRunLoops();

    CGFloat scale = [SnapshotImageScale floatImageScaleForDevice];

    srand(1);

    for (NSUInteger i = 0; i < kSnapshotCount; ++i) {
      test_images_.insert(std::make_pair(
          [[SnapshotIDWrapper alloc]
              initWithSnapshotID:SnapshotID(SessionID::NewUnique().id())],
          GenerateRandomImage(scale)));
    }

    return true;
  }

  ImageFileManager* GetImageFileManager() {
    DCHECK(image_file_manager_);
    return image_file_manager_;
  }

  // Generates an image of `scale`, filled with a random color.
  UIImage* GenerateRandomImage(CGFloat scale) {
    CGSize size = CGSizeMake(kSnapshotPixelSize, kSnapshotPixelSize);
    UIGraphicsImageRendererFormat* format =
        [UIGraphicsImageRendererFormat preferredFormat];
    format.scale = scale;
    format.opaque = NO;

    UIGraphicsImageRenderer* renderer =
        [[UIGraphicsImageRenderer alloc] initWithSize:size format:format];

    return [renderer
        imageWithActions:^(UIGraphicsImageRendererContext* UIContext) {
          CGContextRef context = UIContext.CGContext;
          CGFloat r = rand() / CGFloat(RAND_MAX);
          CGFloat g = rand() / CGFloat(RAND_MAX);
          CGFloat b = rand() / CGFloat(RAND_MAX);
          CGContextSetRGBStrokeColor(context, r, g, b, 1.0);
          CGContextSetRGBFillColor(context, r, g, b, 1.0);
          CGContextFillRect(context, CGRectMake(0.0, 0.0, kSnapshotPixelSize,
                                                kSnapshotPixelSize));
        }];
  }

  void FlushRunLoops() {
    if (!image_file_manager_) {
      return;
    }

    base::RunLoop run_loop;
    base::RunLoop* run_loop_ptr = &run_loop;
    [image_file_manager_ waitForAllTasksForTestingWithCallback:(^() {
                           run_loop_ptr->QuitClosure().Run();
                         })];
    run_loop.Run();
  }

  // This function removes all snapshots stored in disk.
  void ClearAllImages() {
    if (!image_file_manager_) {
      return;
    }

    [image_file_manager_ removeAllImages];
    FlushRunLoops();

    __block BOOL foundImage = NO;
    __block NSUInteger numCallbacks = 0;
    for (auto it = test_images_.begin(); it != test_images_.end(); ++it) {
      __weak SnapshotIDWrapper* snapshot_id = it->first;
      NSURL* image_url =
          [image_file_manager_ imagePathWithSnapshotID:snapshot_id];

      // Checks that the snapshot is not on disk.
      EXPECT_FALSE(
          [[NSFileManager defaultManager] fileExistsAtPath:[image_url path]]);

      // Check that the snapshot is not in the dictionary.
      [image_file_manager_ readImageWithSnapshotID:snapshot_id
                                        completion:^(UIImage* image) {
                                          ++numCallbacks;
                                          if (image) {
                                            foundImage = YES;
                                          }
                                        }];
    }

    // Expect that all the callbacks ran and that none retrieved an image.
    FlushRunLoops();

    EXPECT_EQ(test_images_.size(), numCallbacks);
    EXPECT_FALSE(foundImage);
  }

  // Guesses the order of the color channels in the image.
  // Supports RGB, BGR, RGBA, BGRA, ARGB, ABGR.
  // Returns the position of each channel between 0 and 3.
  void ComputeColorComponents(CGImageRef cgImage,
                              int* red,
                              int* green,
                              int* blue) {
    CGBitmapInfo bitmapInfo = CGImageGetBitmapInfo(cgImage);
    CGImageAlphaInfo alphaInfo = CGImageGetAlphaInfo(cgImage);
    int byteOrder = bitmapInfo & kCGBitmapByteOrderMask;

    *red = 0;
    *green = 1;
    *blue = 2;

    if (alphaInfo == kCGImageAlphaLast ||
        alphaInfo == kCGImageAlphaPremultipliedLast ||
        alphaInfo == kCGImageAlphaNoneSkipLast) {
      *red = 1;
      *green = 2;
      *blue = 3;
    }

    if (byteOrder != kCGBitmapByteOrder32Host) {
      int lastChannel = (CGImageGetBitsPerPixel(cgImage) == 24) ? 2 : 3;
      *red = lastChannel - *red;
      *green = lastChannel - *green;
      *blue = lastChannel - *blue;
    }
  }

  web::WebTaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_directory_;
  base::ScopedTempDir scoped_temp_directory_for_legacy_path_;
  ImageFileManager* image_file_manager_;
  std::map<SnapshotIDWrapper*, UIImage*> test_images_;
};

// Tests that the color of all snapshots in the storage reloaded from disk.
TEST_F(ImageFileManagerTest, CheckImageColors) {
  ImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  // Put all images to disk.
  for (auto it = test_images_.begin(); it != test_images_.end(); ++it) {
    __weak SnapshotIDWrapper* snapshot_id = it->first;
    __weak UIImage* image = it->second;
    [file_manager writeWithImage:image snapshotID:snapshot_id];
  }
  FlushRunLoops();

  for (auto it = test_images_.begin(); it != test_images_.end(); ++it) {
    __weak SnapshotIDWrapper* snapshot_id = it->first;
    __weak UIImage* reference_image = it->second;
    // Check that images are on the disk.
    NSURL* image_url = [file_manager imagePathWithSnapshotID:snapshot_id];
    EXPECT_TRUE(
        [[NSFileManager defaultManager] fileExistsAtPath:[image_url path]]);

    // Check image colors by comparing the first pixel against the reference
    // image.
    UIImage* image = [UIImage imageWithContentsOfFile:[image_url path]];
    CGImageRef cgImage = [image CGImage];
    ASSERT_TRUE(cgImage != nullptr);

    base::apple::ScopedCFTypeRef<CFDataRef> pixelData(
        CGDataProviderCopyData(CGImageGetDataProvider(cgImage)));
    const char* pixels =
        reinterpret_cast<const char*>(CFDataGetBytePtr(pixelData.get()));
    EXPECT_TRUE(pixels);

    CGImageRef referenceCgImage = [reference_image CGImage];
    base::apple::ScopedCFTypeRef<CFDataRef> referenceData(
        CGDataProviderCopyData(CGImageGetDataProvider(referenceCgImage)));
    const char* referencePixels =
        reinterpret_cast<const char*>(CFDataGetBytePtr(referenceData.get()));
    EXPECT_TRUE(referencePixels);

    if (pixels != nil && referencePixels != nil) {
      // Color components may not be in the same order,
      // because of writing to disk and reloading.
      int red, green, blue;
      ComputeColorComponents(cgImage, &red, &green, &blue);

      int referenceRed, referenceGreen, referenceBlue;
      ComputeColorComponents(referenceCgImage, &referenceRed, &referenceGreen,
                             &referenceBlue);

      // Colors may not be exactly the same (compression or rounding errors)
      // thus a small difference is allowed.
      EXPECT_NEAR(referencePixels[referenceRed], pixels[red], 1);
      EXPECT_NEAR(referencePixels[referenceGreen], pixels[green], 1);
      EXPECT_NEAR(referencePixels[referenceBlue], pixels[blue], 1);
    }
  }
}

// Tests that old images are deleted.
TEST_F(ImageFileManagerTest, PurgeImagesOlderThan) {
  ImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  // Put all images in the storage.
  for (auto it = test_images_.begin(); it != test_images_.end(); ++it) {
    __weak SnapshotIDWrapper* snapshot_id = it->first;
    __weak UIImage* image = it->second;
    [file_manager writeWithImage:image snapshotID:snapshot_id];
  }
  FlushRunLoops();

  NSArray* live_snapshot_ids =
      [[NSArray alloc] initWithObjects:test_images_.begin()->first, nil];
  ASSERT_FALSE(test_images_.empty());

  // Purge the storage.
  [file_manager purgeImagesOlderThanWithThresholdDate:
                    [NSDate dateWithTimeIntervalSinceNow:-3600]
                                      liveSnapshotIDs:live_snapshot_ids];
  FlushRunLoops();

  // Check that nothing has been deleted.
  for (auto it = test_images_.begin(); it != test_images_.end(); ++it) {
    __weak SnapshotIDWrapper* snapshot_id = it->first;
    // Check that images are on the disk.
    NSURL* image_url = [file_manager imagePathWithSnapshotID:snapshot_id];
    EXPECT_TRUE(
        [[NSFileManager defaultManager] fileExistsAtPath:[image_url path]]);
  }

  // Purge the storage.
  [file_manager purgeImagesOlderThanWithThresholdDate:[NSDate now]
                                      liveSnapshotIDs:live_snapshot_ids];
  FlushRunLoops();

  // Check that the file have been deleted.
  for (auto it = test_images_.begin(); it != test_images_.end(); ++it) {
    __weak SnapshotIDWrapper* snapshot_id = it->first;
    // Check that images are on the disk.
    NSURL* image_url = [file_manager imagePathWithSnapshotID:snapshot_id];
    if (snapshot_id == live_snapshot_ids[0]) {
      EXPECT_TRUE(
          [[NSFileManager defaultManager] fileExistsAtPath:[image_url path]]);
    } else {
      EXPECT_FALSE(
          [[NSFileManager defaultManager] fileExistsAtPath:[image_url path]]);
    }
  }
}

// Tests that migration code correctly rename the specified files and leave
// the other files untouched.
TEST_F(ImageFileManagerTest, RenameSnapshots) {
  ImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  // This snapshot will be renamed.
  NSString* image1_id = [[NSUUID UUID] UUIDString];
  NSURL* image1_url = [file_manager legacyImagePathWithSnapshotID:image1_id];
  ASSERT_TRUE(base::WriteFile(
      base::FilePath(base::SysNSStringToUTF8([image1_url path])), "image1"));

  // This snapshot will not be renamed.
  NSString* image2_id = [[NSUUID UUID] UUIDString];
  NSURL* image2_url = [file_manager legacyImagePathWithSnapshotID:image2_id];
  ASSERT_TRUE(base::WriteFile(
      base::FilePath(base::SysNSStringToUTF8([image2_url path])), "image2"));

  SnapshotIDWrapper* new_id = [[SnapshotIDWrapper alloc]
      initWithSnapshotID:SnapshotID(SessionID::NewUnique().id())];
  [file_manager renameSnapshotsWithOldIDs:@[ image1_id ] newIDs:@[ new_id ]];
  FlushRunLoops();

  // image1 should have been moved.
  EXPECT_FALSE(
      [[NSFileManager defaultManager] fileExistsAtPath:[image1_url path]]);
  EXPECT_TRUE([[NSFileManager defaultManager]
      fileExistsAtPath:[[file_manager imagePathWithSnapshotID:new_id] path]]);

  // image2 should not have moved.
  EXPECT_TRUE(
      [[NSFileManager defaultManager] fileExistsAtPath:[image2_url path]]);
}

// Tests that image size and scale are preserved when writing and reading
// from disk.
TEST_F(ImageFileManagerTest, SizeAndScalePreservation) {
  ImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  // Create an image with the expected snapshot scale.
  CGFloat scale = [SnapshotImageScale floatImageScaleForDevice];
  UIImage* image = GenerateRandomImage(scale);

  // Add the image to the storage and ensure the file is written to disk.
  SnapshotIDWrapper* snapshot_id = [[SnapshotIDWrapper alloc]
      initWithSnapshotID:SnapshotID(SessionID::NewUnique().id())];
  [file_manager writeWithImage:image snapshotID:snapshot_id];
  FlushRunLoops();

  // Retrive the image and have the callback verify the size and scale.
  __block BOOL callbackComplete = NO;
  [file_manager
      readImageWithSnapshotID:snapshot_id
                   completion:^(UIImage* imageFromDisk) {
                     EXPECT_EQ(image.size.width, imageFromDisk.size.width);
                     EXPECT_EQ(image.size.height, imageFromDisk.size.height);
                     EXPECT_EQ(image.scale, imageFromDisk.scale);
                     callbackComplete = YES;
                   }];

  FlushRunLoops();
  EXPECT_TRUE(callbackComplete);
}

// Tests that retina-scale images are deleted properly.
TEST_F(ImageFileManagerTest, DeleteRetinaImages) {
  ImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  if ([SnapshotImageScale floatImageScaleForDevice] != 2.0) {
    return;
  }

  // Create an image with retina scale.
  UIImage* image = GenerateRandomImage(2.0);

  // Add the image and ensure the file is written to disk.
  SnapshotIDWrapper* snapshot_id = [[SnapshotIDWrapper alloc]
      initWithSnapshotID:SnapshotID(SessionID::NewUnique().id())];
  [file_manager writeWithImage:image snapshotID:snapshot_id];
  FlushRunLoops();

  // Verify the file was written with @2x in the file name.
  NSURL* retinaFile = [file_manager imagePathWithSnapshotID:snapshot_id];
  EXPECT_TRUE(
      [[NSFileManager defaultManager] fileExistsAtPath:[retinaFile path]]);

  // Delete the image and ensure the file is removed.
  [file_manager removeImageWithSnapshotID:snapshot_id];
  FlushRunLoops();

  EXPECT_FALSE(
      [[NSFileManager defaultManager] fileExistsAtPath:[retinaFile path]]);
}

// Tests that an image is immediately deleted when calling
// `-removeImageWithSnapshotID:`.
TEST_F(ImageFileManagerTest, ImageDeleted) {
  ImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  UIImage* image = GenerateRandomImage(0);
  SnapshotIDWrapper* snapshot_id = [[SnapshotIDWrapper alloc]
      initWithSnapshotID:SnapshotID(SessionID::NewUnique().id())];
  [file_manager writeWithImage:image snapshotID:snapshot_id];
  FlushRunLoops();

  NSURL* image_url = [file_manager imagePathWithSnapshotID:snapshot_id];
  EXPECT_TRUE(
      [[NSFileManager defaultManager] fileExistsAtPath:[image_url path]]);

  // Remove the image and ensure the file is removed.
  [file_manager removeImageWithSnapshotID:snapshot_id];
  FlushRunLoops();

  EXPECT_FALSE(
      [[NSFileManager defaultManager] fileExistsAtPath:[image_url path]]);
}

// Tests that all images are deleted when calling `-removeAllImages`.
TEST_F(ImageFileManagerTest, AllImagesDeleted) {
  ImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  UIImage* image = GenerateRandomImage(0);

  SnapshotIDWrapper* snapshot_id1 = [[SnapshotIDWrapper alloc]
      initWithSnapshotID:SnapshotID(SessionID::NewUnique().id())];
  SnapshotIDWrapper* snapshot_id2 = [[SnapshotIDWrapper alloc]
      initWithSnapshotID:SnapshotID(SessionID::NewUnique().id())];

  [file_manager writeWithImage:image snapshotID:snapshot_id1];
  [file_manager writeWithImage:image snapshotID:snapshot_id2];
  FlushRunLoops();

  NSURL* image_url1 = [file_manager imagePathWithSnapshotID:snapshot_id1];
  NSURL* image_url2 = [file_manager imagePathWithSnapshotID:snapshot_id2];
  EXPECT_TRUE(
      [[NSFileManager defaultManager] fileExistsAtPath:[image_url1 path]]);
  EXPECT_TRUE(
      [[NSFileManager defaultManager] fileExistsAtPath:[image_url2 path]]);

  // Remove all images and ensure the files are removed.
  [file_manager removeAllImages];
  FlushRunLoops();

  EXPECT_FALSE(
      [[NSFileManager defaultManager] fileExistsAtPath:[image_url1 path]]);
  EXPECT_FALSE(
      [[NSFileManager defaultManager] fileExistsAtPath:[image_url2 path]]);
}

}  // namespace
