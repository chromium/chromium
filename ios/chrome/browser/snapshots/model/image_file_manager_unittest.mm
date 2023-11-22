// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/image_file_manager.h"

#import <UIKit/UIKit.h>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/functional/callback_forward.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_scale.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

const NSUInteger kSnapshotCount = 10;
const NSUInteger kSnapshotPixelSize = 8;

class ImageFileManagerTest : public PlatformTest {
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

    image_file_manager_ = [[ImageFileManager alloc]
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
  ImageFileManager* image_file_manager_;
  std::map<SnapshotID, UIImage*> test_images_;
};

// Tests that the color of all snapshots in the storage reloaded from disk.
TEST_F(ImageFileManagerTest, CheckImageColors) {
  ImageFileManager* file_manager = GetImageFileManager();
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
TEST_F(ImageFileManagerTest, PurgeImagesOlderThan) {
  ImageFileManager* file_manager = GetImageFileManager();
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
TEST_F(ImageFileManagerTest, RenameSnapshots) {
  ImageFileManager* file_manager = GetImageFileManager();
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
TEST_F(ImageFileManagerTest, SizeAndScalePreservation) {
  ImageFileManager* file_manager = GetImageFileManager();
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
TEST_F(ImageFileManagerTest, DeleteRetinaImages) {
  ImageFileManager* file_manager = GetImageFileManager();
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
TEST_F(ImageFileManagerTest, ImageDeleted) {
  ImageFileManager* file_manager = GetImageFileManager();
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
TEST_F(ImageFileManagerTest, AllImagesDeleted) {
  ImageFileManager* file_manager = GetImageFileManager();
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

class ImageFileManagerWithoutStoringGreySnapshotsTest
    : public ImageFileManagerTest {
 public:
  ImageFileManagerWithoutStoringGreySnapshotsTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kGreySnapshotOptimization,
        {{"level", "do-not-store-to-disk-and-cache"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that all grey images are deleted when ImageFileManager is initialized.
TEST_F(ImageFileManagerWithoutStoringGreySnapshotsTest,
       DeleteExistingGreySnapshotsOnInit) {
  ImageFileManager* file_manager = GetImageFileManager();
  ASSERT_TRUE(file_manager);

  // Write color images to disk.
  UIImage* image = GenerateRandomImage(0);
  const SnapshotID kSnapshotID1(SessionID::NewUnique().id());
  const SnapshotID kSnapshotID2(SessionID::NewUnique().id());
  [file_manager writeImage:image withSnapshotID:kSnapshotID1];
  [file_manager writeImage:image withSnapshotID:kSnapshotID2];

  // Write grey images generated from color ones to disk.
  [file_manager convertAndSaveGreyImage:kSnapshotID1];
  [file_manager convertAndSaveGreyImage:kSnapshotID2];

  // Wait until all operations are done.
  FlushRunLoops();

  base::FilePath image_1_path =
      [file_manager greyImagePathForSnapshotID:kSnapshotID1];
  base::FilePath image_2_path =
      [file_manager greyImagePathForSnapshotID:kSnapshotID2];
  EXPECT_TRUE(base::PathExists(image_1_path));
  EXPECT_TRUE(base::PathExists(image_2_path));

  // Initialize ImageFileManager again so that the existing grey images
  // should be deleted.
  ImageFileManager* file_manager2 = [[ImageFileManager alloc]
      initWithStoragePath:scoped_temp_directory_.GetPath()
               legacyPath:base::FilePath()];
  ASSERT_TRUE(file_manager2);

  // Wait until all grey images are deleted by initializing `file_manager2`.
  base::RunLoop run_loop;
  [file_manager2
      readImageWithSnapshotID:SnapshotID(SessionID::NewUnique().id())
                   completion:base::BindOnce(base::IgnoreArgs<UIImage*>(
                                  run_loop.QuitClosure()))];
  run_loop.Run();

  EXPECT_FALSE(base::PathExists(image_1_path));
  EXPECT_FALSE(base::PathExists(image_2_path));

  [file_manager2 shutdown];
}

}  // namespace
