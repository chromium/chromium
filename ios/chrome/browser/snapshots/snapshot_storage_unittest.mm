// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_storage.h"

#import <UIKit/UIKit.h>

#import "base/apple/scoped_cftyperef.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/format_macros.h"
#import "base/location.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/snapshots/snapshot_id.h"
#import "ios/chrome/browser/snapshots/snapshot_storage_internal.h"
#import "ios/chrome/browser/snapshots/snapshot_storage_observer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_thread.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

const NSUInteger kSnapshotCount = 10;
const NSUInteger kSnapshotPixelSize = 8;

// Constants used to construct path to test the storage migration.
const base::FilePath::CharType kSnapshots[] = FILE_PATH_LITERAL("Snapshots");
const base::FilePath::CharType kSessions[] = FILE_PATH_LITERAL("Sessions");
const base::FilePath::CharType kIdentifier[] = FILE_PATH_LITERAL("Identifier");
const base::FilePath::CharType kFilename[] = FILE_PATH_LITERAL("Filename.txt");

}  // namespace

@interface FakeSnapshotStorageObserver : NSObject <SnapshotStorageObserver>
@property(nonatomic, assign) SnapshotID lastUpdatedID;
@end

@implementation FakeSnapshotStorageObserver
- (void)snapshotStorage:(SnapshotStorage*)snapshotStorage
    didUpdateSnapshotForID:(SnapshotID)snapshotID {
  self.lastUpdatedID = snapshotID;
}
@end

namespace {

class SnapshotStorageTest : public PlatformTest {
 protected:
  void TearDown() override {
    ClearDumpedImages();
    [snapshot_storage_ shutdown];
    snapshot_storage_ = nil;
    PlatformTest::TearDown();
  }

  // Build an array of snapshot IDs and an array of UIImages filled with
  // random colors.
  [[nodiscard]] bool CreateSnapshotStorage() {
    DCHECK(!snapshot_storage_);
    if (!scoped_temp_directory_.CreateUniqueTempDir()) {
      return false;
    }

    snapshot_storage_ = [[SnapshotStorage alloc]
        initWithStoragePath:scoped_temp_directory_.GetPath()];

    CGFloat scale = [snapshot_storage_ snapshotScaleForDevice];

    srand(1);

    for (NSUInteger i = 0; i < kSnapshotCount; ++i) {
      test_images_.insert(std::make_pair(
          SnapshotID(SessionID::NewUnique().id()), GenerateRandomImage(scale)));
    }

    return true;
  }

  SnapshotStorage* GetSnapshotStorage() {
    DCHECK(snapshot_storage_);
    return snapshot_storage_;
  }

  // Adds a fake snapshot file into `directory` using `snapshot_id` in the
  // filename.
  base::FilePath AddSnapshotFileToDirectory(const base::FilePath directory,
                                            SnapshotID snapshot_id) {
    // Use the same filename as designated by SnapshotStorage.
    base::FilePath storage_image_path =
        [GetSnapshotStorage() imagePathForSnapshotID:snapshot_id];
    base::FilePath image_filename = storage_image_path.BaseName();
    base::FilePath image_path = directory.Append(image_filename);

    EXPECT_TRUE(WriteFile(image_path, ""));
    EXPECT_TRUE(base::PathExists(image_path));
    EXPECT_FALSE(base::PathExists(storage_image_path));
    return image_path;
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
  void FlushRunLoops(SnapshotStorage* storage) {
    base::RunLoop run_loop;
    [storage retrieveImageForSnapshotID:SnapshotID(SessionID::NewUnique().id())
                               callback:base::CallbackToBlock(
                                            base::IgnoreArgs<UIImage*>(
                                                run_loop.QuitClosure()))];
    run_loop.Run();
  }

  // This function removes the snapshots both from dictionary and from disk.
  void ClearDumpedImages() {
    if (!snapshot_storage_) {
      return;
    }

    for (auto [snapshot_id, _] : test_images_) {
      [snapshot_storage_ removeImageWithSnapshotID:snapshot_id];
    }

    FlushRunLoops(snapshot_storage_);

    // The above calls to -removeImageWithSnapshotID remove both the color
    // and grey snapshots for each snapshotID, if they are on disk.  However,
    // ensure we also get rid of the grey snapshots in memory.
    [snapshot_storage_ removeGreyCache];

    __block BOOL foundImage = NO;
    __block NSUInteger numCallbacks = 0;
    for (auto [snapshot_id, _] : test_images_) {
      const base::FilePath path =
          [snapshot_storage_ imagePathForSnapshotID:snapshot_id];

      // Checks that the snapshot is not on disk.
      EXPECT_FALSE(base::PathExists(path));

      // Check that the snapshot is not in the dictionary.
      [snapshot_storage_ retrieveImageForSnapshotID:snapshot_id
                                           callback:^(UIImage* image) {
                                             ++numCallbacks;
                                             if (image) {
                                               foundImage = YES;
                                             }
                                           }];
    }

    // Expect that all the callbacks ran and that none retrieved an image.
    FlushRunLoops(snapshot_storage_);

    EXPECT_EQ(test_images_.size(), numCallbacks);
    EXPECT_FALSE(foundImage);
  }

  // Loads kSnapshotCount color images into the storage.  If
  // `waitForFilesOnDisk` is YES, will not return until the images have been
  // written to disk.
  void LoadAllColorImagesIntoCache(bool waitForFilesOnDisk) {
    LoadColorImagesIntoCache(kSnapshotCount, waitForFilesOnDisk);
  }

  // Loads `count` color images into the storage.  If `waitForFilesOnDisk`
  // is YES, will not return until the images have been written to disk.
  void LoadColorImagesIntoCache(NSUInteger count, bool waitForFilesOnDisk) {
    // Put color images in the storage.
    for (auto [snapshot_id, image] : test_images_) {
      @autoreleasepool {
        [snapshot_storage_ setImage:image withSnapshotID:snapshot_id];
      }
    }
    if (waitForFilesOnDisk) {
      FlushRunLoops(snapshot_storage_);
      for (auto [snapshot_id, _] : test_images_) {
        // Check that images are on the disk.
        const base::FilePath path =
            [snapshot_storage_ imagePathForSnapshotID:snapshot_id];
        EXPECT_TRUE(base::PathExists(path));
      }
    }
  }

  // Waits for the first `count` grey images of `test_images_` to be placed in
  // the storage.
  void WaitForGreyImagesInCache(NSUInteger count) {
    FlushRunLoops(snapshot_storage_);
    {
      NSUInteger index = 0;
      for (auto [snapshot_id, _] : test_images_) {
        EXPECT_TRUE([snapshot_storage_ hasGreyImageInMemory:snapshot_id]);
        if (++index >= count) {
          break;
        }
      }
    }
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

  void TriggerMemoryWarning() {
    // _performMemoryWarning is a private API and must not be compiled into
    // official builds.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
    [[UIApplication sharedApplication]
        performSelector:@selector(_performMemoryWarning)];
#pragma clang diagnostic pop
  }

  web::WebTaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_directory_;
  SnapshotStorage* snapshot_storage_;
  std::map<SnapshotID, UIImage*> test_images_;
};

// This test simply put all the snapshots in the storage and then gets them back
// As the snapshots are kept in memory, the same pointer can be retrieved.
// This test also checks that images are correctly removed from the disk.
TEST_F(SnapshotStorageTest, Cache) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();

  NSUInteger expectedCacheSize = MIN(kSnapshotCount, [storage lruCacheMaxSize]);

  // Put all images in the storage.
  {
    NSUInteger inserted_images = 0;
    for (auto [snapshot_id, image] : test_images_) {
      [storage setImage:image withSnapshotID:snapshot_id];
      if (++inserted_images == expectedCacheSize) {
        break;
      }
    }
  }

  // Get images back.
  __block NSUInteger numberOfCallbacks = 0;
  {
    NSUInteger inserted_images = 0;
    for (auto [snapshot_id, image] : test_images_) {
      UIImage* expected_image = image;
      [storage retrieveImageForSnapshotID:snapshot_id
                                 callback:^(UIImage* retrieved_image) {
                                   // Images have not been removed from the
                                   // dictionnary. We expect the same pointer.
                                   EXPECT_EQ(retrieved_image, expected_image);
                                   ++numberOfCallbacks;
                                 }];
      if (++inserted_images == expectedCacheSize) {
        break;
      }
    }
  }

  EXPECT_EQ(expectedCacheSize, numberOfCallbacks);
}

// This test puts all the snapshots in the storage and flushes them to disk.
// The snapshots are then reloaded from the disk, and the colors are compared.
TEST_F(SnapshotStorageTest, SaveToDisk) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();

  // Put all images in the storage.
  for (auto [snapshot_id, image] : test_images_) {
    [storage setImage:image withSnapshotID:snapshot_id];
  }
  FlushRunLoops(storage);

  for (auto [snapshot_id, reference_image] : test_images_) {
    // Check that images are on the disk.
    const base::FilePath path = [storage imagePathForSnapshotID:snapshot_id];
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
        reinterpret_cast<const char*>(CFDataGetBytePtr(pixelData));
    EXPECT_TRUE(pixels);

    CGImageRef referenceCgImage = [reference_image CGImage];
    base::apple::ScopedCFTypeRef<CFDataRef> referenceData(
        CGDataProviderCopyData(CGImageGetDataProvider(referenceCgImage)));
    const char* referencePixels =
        reinterpret_cast<const char*>(CFDataGetBytePtr(referenceData));
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

TEST_F(SnapshotStorageTest, Purge) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();

  // Put all images in the storage.
  for (auto [snapshot_id, image] : test_images_) {
    [storage setImage:image withSnapshotID:snapshot_id];
  }

  ASSERT_FALSE(test_images_.empty());
  std::vector<SnapshotID> liveSnapshotIDs = {test_images_.begin()->first};

  // Purge the storage.
  [storage purgeCacheOlderThan:(base::Time::Now() - base::Hours(1))
                       keeping:liveSnapshotIDs];
  FlushRunLoops(storage);

  // Check that nothing has been deleted.
  for (auto [snapshot_id, _] : test_images_) {
    // Check that images are on the disk.
    const base::FilePath path = [storage imagePathForSnapshotID:snapshot_id];
    EXPECT_TRUE(base::PathExists(path));
  }

  // Purge the storage.
  [storage purgeCacheOlderThan:base::Time::Now() keeping:liveSnapshotIDs];
  FlushRunLoops(storage);

  // Check that the file have been deleted.
  for (auto [snapshot_id, _] : test_images_) {
    // Check that images are on the disk.
    const base::FilePath path = [storage imagePathForSnapshotID:snapshot_id];
    if (snapshot_id == *liveSnapshotIDs.begin()) {
      EXPECT_TRUE(base::PathExists(path));
    } else {
      EXPECT_FALSE(base::PathExists(path));
    }
  }
}

// Tests that migration code correctly rename the specified files and leave
// the other files untouched.
TEST_F(SnapshotStorageTest, RenameSnapshots) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();

  // This snapshot will be renamed.
  NSString* image1_id = [[NSUUID UUID] UUIDString];
  base::FilePath image1_path = [storage legacyImagePathForSnapshotID:image1_id];
  ASSERT_TRUE(base::WriteFile(image1_path, "image1"));

  // This snapshot will not be renamed.
  NSString* image2_id = [[NSUUID UUID] UUIDString];
  base::FilePath image2_path = [storage legacyImagePathForSnapshotID:image2_id];
  ASSERT_TRUE(base::WriteFile(image2_path, "image2"));

  SnapshotID new_id = SnapshotID(SessionID::NewUnique().id());
  [storage renameSnapshotsWithIDs:@[ image1_id ] toIDs:{new_id}];
  FlushRunLoops(storage);

  // image1 should have been moved.
  EXPECT_FALSE(base::PathExists(image1_path));
  EXPECT_TRUE(base::PathExists([storage imagePathForSnapshotID:new_id]));

  // image2 should not have moved.
  EXPECT_TRUE(base::PathExists(image2_path));
}

// Tests that createGreyCache creates the grey snapshots in the background,
// from color images in the in-memory storage.  When the grey images are all
// loaded into memory, tests that the request to retrieve the grey snapshot
// calls the callback immediately.
// Disabled on simulators because it sometimes crashes. crbug/421425
#if !TARGET_IPHONE_SIMULATOR
TEST_F(SnapshotStorageTest, CreateGreyCache) {
  ASSERT_TRUE(CreateSnapshotStorage());
  LoadAllColorImagesIntoCache(true);

  // Request the creation of a grey image storage for all images.
  SnapshotStorage* storage = GetSnapshotStorage();
  {
    std::vector<SnapshotID> snapshot_ids;
    for (auto [snapshot_id, _] : test_images_) {
      snapshot_ids.push_back(snapshot_id);
    }
    [storage createGreyCache:snapshot_ids];
  }

  // Wait for them to be put into the grey image storage.
  WaitForGreyImagesInCache(kSnapshotCount);

  __block NSUInteger numberOfCallbacks = 0;
  for (auto [snapshot_id, _] : test_images_) {
    [storage retrieveGreyImageForSnapshotID:snapshot_id
                                   callback:^(UIImage* image) {
                                     EXPECT_TRUE(image);
                                     ++numberOfCallbacks;
                                   }];
  }

  EXPECT_EQ(numberOfCallbacks, kSnapshotCount);
}

// Same as previous test, except that all the color images are on disk,
// rather than in memory.
// Disabled due to the greyImage crash.  b/8048597
TEST_F(SnapshotStorageTest, CreateGreyCacheFromDisk) {
  ASSERT_TRUE(CreateSnapshotStorage());
  LoadAllColorImagesIntoCache(true);

  // Remove color images from in-memory storage.
  SnapshotStorage* storage = GetSnapshotStorage();

  TriggerMemoryWarning();

  // Request the creation of a grey image storage for all images.
  {
    std::vector<SnapshotID> snapshot_ids;
    for (auto [snapshot_id, _] : test_images_) {
      snapshot_ids.push_back(snapshot_id);
    }
    [storage createGreyCache:snapshot_ids];
  }

  // Wait for them to be put into the grey image storage.
  WaitForGreyImagesInCache(kSnapshotCount);

  __block NSUInteger numberOfCallbacks = 0;
  for (auto [snapshot_id, _] : test_images_) {
    [storage retrieveGreyImageForSnapshotID:snapshot_id
                                   callback:^(UIImage* image) {
                                     EXPECT_TRUE(image);
                                     ++numberOfCallbacks;
                                   }];
  }

  EXPECT_EQ(numberOfCallbacks, kSnapshotCount);
}
#endif  // !TARGET_IPHONE_SIMULATOR

// Tests mostRecentGreyBlock, which is a block to be called when the most
// recently requested grey image is finally loaded.
// The test requests three images be storaged as grey images.  Only the final
// callback of the three requests should be called.
// Disabled due to the greyImage crash.  b/8048597
TEST_F(SnapshotStorageTest, MostRecentGreyBlock) {
  ASSERT_TRUE(CreateSnapshotStorage());
  const NSUInteger kNumImages = 3;
  std::vector<SnapshotID> snapshotIDs;
  for (auto [snapshot_id, _] : test_images_) {
    snapshotIDs.push_back(snapshot_id);
    if (snapshotIDs.size() >= kNumImages) {
      break;
    }
  }

  SnapshotStorage* storage = GetSnapshotStorage();

  // Put 3 images in the storage.
  LoadColorImagesIntoCache(kNumImages, true);

  // Make sure the color images are only on disk, to ensure the background
  // thread is slow enough to queue up the requests.
  TriggerMemoryWarning();

  // Enable the grey image storage.
  [storage createGreyCache:snapshotIDs];

  // Request the grey versions
  __block BOOL firstCallbackCalled = NO;
  __block BOOL secondCallbackCalled = NO;
  __block BOOL thirdCallbackCalled = NO;
  ASSERT_GE(snapshotIDs.size(), kNumImages);
  [storage greyImageForSnapshotID:snapshotIDs[0]
                         callback:^(UIImage*) {
                           firstCallbackCalled = YES;
                         }];
  [storage greyImageForSnapshotID:snapshotIDs[1]
                         callback:^(UIImage*) {
                           secondCallbackCalled = YES;
                         }];
  [storage greyImageForSnapshotID:snapshotIDs[2]
                         callback:^(UIImage*) {
                           thirdCallbackCalled = YES;
                         }];

  // Wait for them to be loaded.
  WaitForGreyImagesInCache(kNumImages);

  EXPECT_FALSE(firstCallbackCalled);
  EXPECT_FALSE(secondCallbackCalled);
  EXPECT_TRUE(thirdCallbackCalled);
}

// Test the function used to save a grey copy of a color snapshot fully on a
// background thread when the application is backgrounded.
TEST_F(SnapshotStorageTest, GreyImageAllInBackground) {
  ASSERT_TRUE(CreateSnapshotStorage());
  LoadAllColorImagesIntoCache(true);

  SnapshotStorage* storage = GetSnapshotStorage();

  // Now convert every image into a grey image, on disk, in the background.
  for (auto [snapshot_id, _] : test_images_) {
    [storage saveGreyInBackgroundForSnapshotID:snapshot_id];
  }

  // Waits for the grey images for `test_images_` to be written to disk, which
  // happens in a background thread.
  FlushRunLoops(storage);

  for (auto [snapshot_id, _] : test_images_) {
    const base::FilePath path =
        [storage greyImagePathForSnapshotID:snapshot_id];
    EXPECT_TRUE(base::PathExists(path));
    base::DeleteFile(path);
  }
}

// Verifies that image size and scale are preserved when writing and reading
// from disk.
TEST_F(SnapshotStorageTest, SizeAndScalePreservation) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();

  // Create an image with the expected snapshot scale.
  CGFloat scale = [storage snapshotScaleForDevice];
  UIImage* image = GenerateRandomImage(scale);

  // Add the image to the storage then call handle low memory to ensure the
  // image is read from disk instead of the in-memory storage.
  const SnapshotID kSnapshotID(SessionID::NewUnique().id());
  [storage setImage:image withSnapshotID:kSnapshotID];
  FlushRunLoops(storage);  // ensure the file is written to disk.
  TriggerMemoryWarning();

  // Retrive the image and have the callback verify the size and scale.
  __block BOOL callbackComplete = NO;
  [storage
      retrieveImageForSnapshotID:kSnapshotID
                        callback:^(UIImage* imageFromDisk) {
                          EXPECT_EQ(image.size.width, imageFromDisk.size.width);
                          EXPECT_EQ(image.size.height,
                                    imageFromDisk.size.height);
                          EXPECT_EQ(image.scale, imageFromDisk.scale);
                          callbackComplete = YES;
                        }];
  FlushRunLoops(storage);
  EXPECT_TRUE(callbackComplete);
}

// Verifies that retina-scale images are deleted properly.
TEST_F(SnapshotStorageTest, DeleteRetinaImages) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();
  if ([storage snapshotScaleForDevice] != 2.0) {
    return;
  }

  // Create an image with retina scale.
  UIImage* image = GenerateRandomImage(2.0);

  // Add the image to the storage then call handle low memory to ensure the
  // image is read from disk instead of the in-memory storage.
  const SnapshotID kSnapshotID(SessionID::NewUnique().id());
  [storage setImage:image withSnapshotID:kSnapshotID];
  FlushRunLoops(storage);  // ensure the file is written to disk.
  TriggerMemoryWarning();

  // Verify the file was writted with @2x in the file name.
  base::FilePath retinaFile = [storage imagePathForSnapshotID:kSnapshotID];
  EXPECT_TRUE(base::PathExists(retinaFile));

  // Delete the image.
  [storage removeImageWithSnapshotID:kSnapshotID];
  FlushRunLoops(storage);  // ensure the file is removed.

  EXPECT_FALSE(base::PathExists(retinaFile));
}

// Tests that image immediately deletes when calling
// `-removeImageWithSnapshotID:`.
TEST_F(SnapshotStorageTest, ImageDeleted) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();
  UIImage* image = GenerateRandomImage(0);
  const SnapshotID kSnapshotID(SessionID::NewUnique().id());
  [storage setImage:image withSnapshotID:kSnapshotID];
  base::FilePath image_path = [storage imagePathForSnapshotID:kSnapshotID];
  [storage removeImageWithSnapshotID:kSnapshotID];
  // Give enough time for deletion.
  FlushRunLoops(storage);
  EXPECT_FALSE(base::PathExists(image_path));
}

// Tests that all images are deleted when calling `-removeAllImages`.
TEST_F(SnapshotStorageTest, AllImagesDeleted) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();
  UIImage* image = GenerateRandomImage(0);
  const SnapshotID kSnapshotID1(SessionID::NewUnique().id());
  const SnapshotID kSnapshotID2(SessionID::NewUnique().id());
  [storage setImage:image withSnapshotID:kSnapshotID1];
  [storage setImage:image withSnapshotID:kSnapshotID1];
  base::FilePath image_1_path = [storage imagePathForSnapshotID:kSnapshotID1];
  base::FilePath image_2_path = [storage imagePathForSnapshotID:kSnapshotID1];
  [storage removeAllImages];
  // Give enough time for deletion.
  FlushRunLoops(storage);
  EXPECT_FALSE(base::PathExists(image_1_path));
  EXPECT_FALSE(base::PathExists(image_2_path));
}

// Tests that observers are notified when a snapshot is storaged and removed.
TEST_F(SnapshotStorageTest, ObserversNotifiedOnSetAndRemoveImage) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();
  FakeSnapshotStorageObserver* observer =
      [[FakeSnapshotStorageObserver alloc] init];
  [storage addObserver:observer];
  EXPECT_FALSE(observer.lastUpdatedID.valid());
  ASSERT_TRUE(!test_images_.empty());
  std::pair<SnapshotID, UIImage*> pair = *test_images_.begin();
  [storage setImage:pair.second withSnapshotID:pair.first];
  EXPECT_EQ(pair.first, observer.lastUpdatedID);
  observer.lastUpdatedID = SnapshotID();
  [storage removeImageWithSnapshotID:pair.first];
  EXPECT_EQ(pair.first, observer.lastUpdatedID);
  [storage removeObserver:observer];
}

// Tests that creating the SnapshotStorage migrate an existing legacy storage.
TEST_F(SnapshotStorageTest, MigrateCache) {
  ASSERT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_directory_.GetPath();

  const base::FilePath storage_path =
      root.Append(kSnapshots).Append(kIdentifier);

  const base::FilePath legacy_path =
      root.Append(kSessions).Append(kIdentifier).Append(kSnapshots);

  ASSERT_TRUE(base::CreateDirectory(legacy_path));
  ASSERT_TRUE(base::WriteFile(legacy_path.Append(kFilename), ""));

  SnapshotStorage* storage =
      [[SnapshotStorage alloc] initWithStoragePath:storage_path
                                        legacyPath:legacy_path];

  FlushRunLoops(storage);

  EXPECT_TRUE(base::DirectoryExists(storage_path));
  EXPECT_FALSE(base::DirectoryExists(legacy_path));

  // Check that the legacy directory content has been moved.
  EXPECT_TRUE(base::PathExists(storage_path.Append(kFilename)));

  [storage shutdown];
}

// Tests that creating the SnapshotStorage simply create the storage directory
// if the legacy path is not specified.
TEST_F(SnapshotStorageTest, MigrateCache_EmptyLegacyPath) {
  ASSERT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_directory_.GetPath();

  const base::FilePath storage_path =
      root.Append(kSnapshots).Append(kIdentifier);

  SnapshotStorage* storage =
      [[SnapshotStorage alloc] initWithStoragePath:storage_path
                                        legacyPath:base::FilePath()];

  FlushRunLoops(storage);

  EXPECT_TRUE(base::DirectoryExists(storage_path));

  [storage shutdown];
}

// Tests that creating the SnapshotStorage simply create the storage directory
// if the legacy path does not exists.
TEST_F(SnapshotStorageTest, MigrateCache_NoLegacyStorage) {
  ASSERT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_directory_.GetPath();

  const base::FilePath storage_path =
      root.Append(kSnapshots).Append(kIdentifier);

  const base::FilePath legacy_path =
      root.Append(kSessions).Append(kIdentifier).Append(kSnapshots);

  ASSERT_FALSE(base::DirectoryExists(legacy_path));

  SnapshotStorage* storage =
      [[SnapshotStorage alloc] initWithStoragePath:storage_path
                                        legacyPath:legacy_path];

  FlushRunLoops(storage);

  EXPECT_TRUE(base::DirectoryExists(storage_path));
  EXPECT_FALSE(base::DirectoryExists(legacy_path));

  [storage shutdown];
}

// Tests that creating the SnapshotStorage can fail to create the storage
// directory and that the legacy directory is left untouch in that case.
TEST_F(SnapshotStorageTest, MigrateCache_FailCreatingCache) {
  ASSERT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_directory_.GetPath();

  const base::FilePath storage_path =
      root.Append(kSnapshots).Append(kIdentifier);

  const base::FilePath legacy_path =
      root.Append(kSessions).Append(kIdentifier).Append(kSnapshots);

  ASSERT_TRUE(base::CreateDirectory(legacy_path));
  ASSERT_TRUE(base::WriteFile(legacy_path.Append(kFilename), ""));

  // Create a file with the same name as the storage directory to
  // simulate a failure (in real world the failure would be caused
  // by a disk that is full).
  ASSERT_TRUE(base::CreateDirectory(storage_path.DirName()));
  ASSERT_TRUE(base::WriteFile(storage_path, ""));

  SnapshotStorage* storage =
      [[SnapshotStorage alloc] initWithStoragePath:storage_path
                                        legacyPath:base::FilePath()];

  FlushRunLoops(storage);

  EXPECT_FALSE(base::DirectoryExists(storage_path));
  EXPECT_TRUE(base::DirectoryExists(legacy_path));
  EXPECT_TRUE(base::PathExists(legacy_path.Append(kFilename)));

  [storage shutdown];
}

}  // namespace
