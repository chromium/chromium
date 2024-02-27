// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_storage.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage+Testing.h"

#import <UIKit/UIKit.h>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_scale.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_observer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

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

const NSUInteger kSnapshotCount = 10;
const NSUInteger kSnapshotPixelSize = 8;

// Constants used to construct path to test the storage migration.
const base::FilePath::CharType kSnapshots[] = FILE_PATH_LITERAL("Snapshots");
const base::FilePath::CharType kSessions[] = FILE_PATH_LITERAL("Sessions");
const base::FilePath::CharType kIdentifier[] = FILE_PATH_LITERAL("Identifier");
const base::FilePath::CharType kFilename[] = FILE_PATH_LITERAL("Filename.txt");

class SnapshotStorageTest : public PlatformTest {
 protected:
  void TearDown() override {
    ClearAllImages();
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

    CGFloat scale = [SnapshotImageScale floatImageScaleForDevice];

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
  void ClearAllImages() {
    if (!snapshot_storage_) {
      return;
    }

    for (auto [snapshot_id, _] : test_images_) {
      [snapshot_storage_ removeImageWithSnapshotID:snapshot_id];
    }

    FlushRunLoops(snapshot_storage_);

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

// This test simply put all snapshots in the storage and then gets them back.
// As the snapshots are kept in memory, the same pointer can be retrieved.
// This test also checks that images are correctly removed from the disk.
TEST_F(SnapshotStorageTest, ReadAndWriteCache) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();

  NSUInteger expectedCacheSize = MIN(kSnapshotCount, [storage lruCacheMaxSize]);

  // Put all images to the cache.
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

// This test puts all snapshots in the storage, clears the LRU cache and checks
// if the image can be retrieved via disk.
TEST_F(SnapshotStorageTest, ReadAndWriteWithoutCache) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();

  LoadAllColorImagesIntoCache(true);

  // Remove color images from LRU cache.
  [storage clearCache];

  __block NSUInteger numberOfCallbacks = 0;

  for (auto [snapshot_id, _] : test_images_) {
    // Check that images are on the disk.
    const base::FilePath path = [storage imagePathForSnapshotID:snapshot_id];
    EXPECT_TRUE(base::PathExists(path));

    [storage retrieveImageForSnapshotID:snapshot_id
                               callback:^(UIImage* image) {
                                 EXPECT_TRUE(image);
                                 ++numberOfCallbacks;
                               }];
  }

  // Wait until all callbacks are called.
  FlushRunLoops(storage);

  EXPECT_EQ(numberOfCallbacks, kSnapshotCount);
}

// Tests that an image is immediately deleted when calling
// `-removeImageWithSnapshotID:`.
TEST_F(SnapshotStorageTest, ImageDeleted) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();

  UIImage* image = GenerateRandomImage(0);
  const SnapshotID kSnapshotID(SessionID::NewUnique().id());
  [storage setImage:image withSnapshotID:kSnapshotID];

  base::FilePath image_path = [storage imagePathForSnapshotID:kSnapshotID];

  // Remove the image and ensure the file is removed.
  [storage removeImageWithSnapshotID:kSnapshotID];
  FlushRunLoops(storage);

  EXPECT_FALSE(base::PathExists(image_path));
  [storage retrieveImageForSnapshotID:kSnapshotID
                             callback:^(UIImage* retrievedImage) {
                               EXPECT_FALSE(retrievedImage);
                             }];
}

// Tests that all images are deleted when calling `-removeAllImages`.
TEST_F(SnapshotStorageTest, AllImagesDeleted) {
  ASSERT_TRUE(CreateSnapshotStorage());
  SnapshotStorage* storage = GetSnapshotStorage();

  UIImage* image = GenerateRandomImage(0);
  const SnapshotID kSnapshotID1(SessionID::NewUnique().id());
  const SnapshotID kSnapshotID2(SessionID::NewUnique().id());
  [storage setImage:image withSnapshotID:kSnapshotID1];
  [storage setImage:image withSnapshotID:kSnapshotID2];
  base::FilePath image_1_path = [storage imagePathForSnapshotID:kSnapshotID1];
  base::FilePath image_2_path = [storage imagePathForSnapshotID:kSnapshotID2];

  // Remove all images and ensure the files are removed.
  [storage removeAllImages];
  FlushRunLoops(storage);

  EXPECT_FALSE(base::PathExists(image_1_path));
  EXPECT_FALSE(base::PathExists(image_2_path));
  [storage retrieveImageForSnapshotID:kSnapshotID1
                             callback:^(UIImage* retrievedImage1) {
                               EXPECT_FALSE(retrievedImage1);
                             }];
  [storage retrieveImageForSnapshotID:kSnapshotID2
                             callback:^(UIImage* retrievedImage2) {
                               EXPECT_FALSE(retrievedImage2);
                             }];
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

// Tests that retrieving grey snapshot images generated by color images stored
// in cache.
TEST_F(SnapshotStorageTest, RetrieveGreyImageFromColorImageInMemory) {
  ASSERT_TRUE(CreateSnapshotStorage());
  LoadAllColorImagesIntoCache(true);

  SnapshotStorage* storage = GetSnapshotStorage();
  __block NSUInteger numberOfCallbacks = 0;
  for (auto [snapshot_id, _] : test_images_) {
    [storage retrieveGreyImageForSnapshotID:snapshot_id
                                   callback:^(UIImage* image) {
                                     EXPECT_TRUE(image);
                                     ++numberOfCallbacks;
                                   }];
  }

  // Wait until all callbacks are called.
  FlushRunLoops(storage);

  EXPECT_EQ(numberOfCallbacks, kSnapshotCount);
}

// Tests that retrieving grey snapshot images generated by color images stored
// in disk.
TEST_F(SnapshotStorageTest, RetrieveGreyImageFromColorImageInDisk) {
  ASSERT_TRUE(CreateSnapshotStorage());
  LoadAllColorImagesIntoCache(true);

  SnapshotStorage* storage = GetSnapshotStorage();

  // Remove color images from in-memory storage.
  [storage clearCache];

  __block NSUInteger numberOfCallbacks = 0;
  for (auto [snapshot_id, _] : test_images_) {
    [storage retrieveGreyImageForSnapshotID:snapshot_id
                                   callback:^(UIImage* image) {
                                     EXPECT_TRUE(image);
                                     ++numberOfCallbacks;
                                   }];
  }

  // Wait until all callbacks are called.
  FlushRunLoops(storage);

  EXPECT_EQ(numberOfCallbacks, kSnapshotCount);
}

}  // namespace
