// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/snapshots/model/constants.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_lru_cache.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_storage.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_scale.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeSnapshotStorageObserver : NSObject <SnapshotStorageObserver>
@property(nonatomic, strong) SnapshotIDWrapper* lastUpdatedID;
@end

@implementation FakeSnapshotStorageObserver
- (void)didUpdateSnapshotStorageWithSnapshotID:(SnapshotIDWrapper*)snapshotID {
  self.lastUpdatedID = snapshotID;
}
@end

namespace {

// Represents the possible SnapshotStorage implementations.
enum class SnapshotStorageKind {
  kLegacy,
  kSwift,
};

const NSInteger kSnapshotCount = 10;
const NSUInteger kSnapshotPixelSize = 8;
const NSUInteger kSnapshotCacheSize = 3;

// Constants used to construct path to test the storage migration.
const base::FilePath::CharType kIdentifier[] = FILE_PATH_LITERAL("Identifier");

// Converts `snapshot_id` into a SnapshotIDWrapper.
SnapshotIDWrapper* ToWrapper(SnapshotID snapshot_id) {
  return [[SnapshotIDWrapper alloc] initWithSnapshotID:snapshot_id];
}

// Returns a new SnapshotIDWrapper instance.
SnapshotIDWrapper* NewSnapshotID() {
  return ToWrapper(SnapshotID(SessionID::NewUnique().id()));
}

// Returns an invalid SnapshotIDWrapper instance.
SnapshotIDWrapper* InvalidSnapshotID() {
  return ToWrapper(SnapshotID());
}

}  // namespace

class SnapshotStorageTest
    : public PlatformTest,
      public testing::WithParamInterface<SnapshotStorageKind> {
 protected:
  void TearDown() override {
    ClearAllImages();
    [snapshot_storage_ shutdown];
    snapshot_storage_ = nil;
    PlatformTest::TearDown();
  }

  // Builds an array of snapshot IDs and an array of UIImages filled with
  // random colors.
  [[nodiscard]] bool CreateSnapshotStorage() {
    DCHECK(!snapshot_storage_);
    if (!scoped_temp_directory_.CreateUniqueTempDir()) {
      return false;
    }

    const base::FilePath storage_path = scoped_temp_directory_.GetPath();
    switch (GetParam()) {
      case SnapshotStorageKind::kLegacy: {
        legacy_lru_cache_ = [[LegacySnapshotLRUCache alloc]
            initWithCacheSize:kSnapshotCacheSize];
        snapshot_storage_ =
            [[LegacySnapshotStorage alloc] initWithLRUCache:legacy_lru_cache_
                                                storagePath:storage_path];
        break;
      }

      case SnapshotStorageKind::kSwift: {
        using base::apple::FilePathToNSURL;
        lru_cache_ = [[SnapshotLRUCache alloc] initWithSize:kSnapshotCacheSize];
        snapshot_storage_ = [[SnapshotStorageImpl alloc]
               initWithLruCache:lru_cache_
            storageDirectoryUrl:FilePathToNSURL(storage_path)];
        break;
      }
    }
    DCHECK(snapshot_storage_);

    CGFloat scale = [SnapshotImageScale floatImageScaleForDevice];

    srand(1);

    test_images_ = [[NSMutableDictionary alloc] init];
    for (auto i = 0; i < kSnapshotCount; ++i) {
      test_images_[NewSnapshotID()] = GenerateRandomImage(scale);
    }

    return true;
  }

  id<SnapshotStorage> GetSnapshotStorage() {
    DCHECK(snapshot_storage_);
    return snapshot_storage_;
  }

  void ClearCache() {
    [legacy_lru_cache_ removeAllObjects];
    [lru_cache_ removeAllObjects];
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
  void FlushRunLoops(id<SnapshotStorage> storage) {
    base::RunLoop run_loop;
    [storage retrieveImageWithSnapshotID:NewSnapshotID()
                            snapshotKind:SnapshotKindColor
                              completion:base::CallbackToBlock(
                                             base::IgnoreArgs<UIImage*>(
                                                 run_loop.QuitClosure()))];
    run_loop.Run();
  }

  // This function removes the snapshots both from the cache and from the disk.
  void ClearAllImages() {
    if (!snapshot_storage_) {
      return;
    }

    for (SnapshotIDWrapper* snapshot_id in test_images_) {
      [snapshot_storage_ removeImageWithSnapshotID:snapshot_id];
    }

    FlushRunLoops(snapshot_storage_);

    __block BOOL foundImage = NO;
    __block NSUInteger numCallbacks = 0;
    for (SnapshotIDWrapper* snapshot_id in test_images_) {
      const NSURL* url =
          [snapshot_storage_ imagePathWithSnapshotID:snapshot_id];

      // Checks that the snapshot is not on disk.
      EXPECT_FALSE(
          [[NSFileManager defaultManager] fileExistsAtPath:[url path]]);

      // Check that the snapshot is not in the dictionary.
      [snapshot_storage_ retrieveImageWithSnapshotID:snapshot_id
                                        snapshotKind:SnapshotKindColor
                                          completion:^(UIImage* image) {
                                            ++numCallbacks;
                                            if (image) {
                                              foundImage = YES;
                                            }
                                          }];
    }

    // Expect that all the callbacks ran and that none retrieved an image.
    FlushRunLoops(snapshot_storage_);

    EXPECT_EQ(test_images_.count, numCallbacks);
    EXPECT_FALSE(foundImage);
  }

  // Loads kSnapshotCount color images into the storage.  If
  // `waitForFilesOnDisk` is YES, will not return until the images have been
  // written to disk.
  void LoadAllColorImagesIntoCache(bool waitForFilesOnDisk) {
    // Put color images in the storage.
    @autoreleasepool {
      for (SnapshotIDWrapper* snapshot_id in test_images_) {
        UIImage* image = test_images_[snapshot_id];
        [snapshot_storage_ setImage:image withSnapshotID:snapshot_id];
      }
    }
    if (waitForFilesOnDisk) {
      FlushRunLoops(snapshot_storage_);
      for (SnapshotIDWrapper* snapshot_id in test_images_) {
        // Check that images are on the disk.
        const NSURL* url =
            [snapshot_storage_ imagePathWithSnapshotID:snapshot_id];
        EXPECT_TRUE(
            [[NSFileManager defaultManager] fileExistsAtPath:[url path]]);
      }
    }
  }

  web::WebTaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_directory_;
  id<SnapshotStorage> snapshot_storage_;
  LegacySnapshotLRUCache* legacy_lru_cache_;
  SnapshotLRUCache* lru_cache_;
  NSMutableDictionary<SnapshotIDWrapper*, UIImage*>* test_images_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SnapshotStorageTest,
                         testing::Values(SnapshotStorageKind::kLegacy,
                                         SnapshotStorageKind::kSwift));

// This test simply put all snapshots in the storage and then gets them back.
// As the snapshots are kept in memory, the same pointer can be retrieved.
TEST_P(SnapshotStorageTest, ReadAndWriteCache) {
  ASSERT_TRUE(CreateSnapshotStorage());
  id<SnapshotStorage> storage = GetSnapshotStorage();

  // Put all images to the cache.
  {
    NSUInteger inserted_images = 0;
    for (SnapshotIDWrapper* snapshot_id in test_images_) {
      UIImage* image = test_images_[snapshot_id];
      [storage setImage:image withSnapshotID:snapshot_id];
      if (++inserted_images == kSnapshotCacheSize) {
        break;
      }
    }
  }

  // Get images back.
  __block NSUInteger numberOfCallbacks = 0;
  {
    NSUInteger inserted_images = 0;
    for (SnapshotIDWrapper* snapshot_id in test_images_) {
      UIImage* expected_image = test_images_[snapshot_id];
      [storage retrieveImageWithSnapshotID:snapshot_id
                              snapshotKind:SnapshotKindColor
                                completion:^(UIImage* retrieved_image) {
                                  // Images have not been removed from the
                                  // cache. We expect the same pointer.
                                  EXPECT_EQ(retrieved_image, expected_image);
                                  ++numberOfCallbacks;
                                }];
      if (++inserted_images == kSnapshotCacheSize) {
        break;
      }
    }
  }

  // Wait until all callbacks are called.
  FlushRunLoops(storage);

  EXPECT_EQ(kSnapshotCacheSize, numberOfCallbacks);
}

// This test puts all snapshots in the storage, clears the LRU cache and checks
// if the images can be retrieved from the disk.
TEST_P(SnapshotStorageTest, ReadAndWriteWithoutCache) {
  ASSERT_TRUE(CreateSnapshotStorage());
  id<SnapshotStorage> storage = GetSnapshotStorage();

  LoadAllColorImagesIntoCache(true);

  // Remove color images from LRU cache.
  ClearCache();

  __block NSInteger numberOfCallbacks = 0;
  for (SnapshotIDWrapper* snapshot_id in test_images_) {
    // Check that images are on the disk.
    const NSURL* url = [storage imagePathWithSnapshotID:snapshot_id];
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:[url path]]);

    [storage retrieveImageWithSnapshotID:snapshot_id
                            snapshotKind:SnapshotKindColor
                              completion:^(UIImage* image) {
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
TEST_P(SnapshotStorageTest, ImageDeleted) {
  ASSERT_TRUE(CreateSnapshotStorage());
  id<SnapshotStorage> storage = GetSnapshotStorage();

  UIImage* image = GenerateRandomImage(0);
  SnapshotIDWrapper* new_snapshot_id = NewSnapshotID();
  [storage setImage:image withSnapshotID:new_snapshot_id];

  NSURL* url = [storage imagePathWithSnapshotID:new_snapshot_id];

  // Remove the image and ensure the file is removed.
  [storage removeImageWithSnapshotID:new_snapshot_id];
  FlushRunLoops(storage);

  EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:[url path]]);
  [storage retrieveImageWithSnapshotID:new_snapshot_id
                          snapshotKind:SnapshotKindColor
                            completion:^(UIImage* retrievedImage) {
                              EXPECT_FALSE(retrievedImage);
                            }];
}

// Tests that all images are deleted when calling `-removeAllImages`.
TEST_P(SnapshotStorageTest, AllImagesDeleted) {
  ASSERT_TRUE(CreateSnapshotStorage());
  id<SnapshotStorage> storage = GetSnapshotStorage();

  UIImage* image = GenerateRandomImage(0);
  SnapshotIDWrapper* new_snapshot_id1 = NewSnapshotID();
  SnapshotIDWrapper* new_snapshot_id2 = NewSnapshotID();
  [storage setImage:image withSnapshotID:new_snapshot_id1];
  [storage setImage:image withSnapshotID:new_snapshot_id2];
  NSURL* url1 = [storage imagePathWithSnapshotID:new_snapshot_id1];
  NSURL* url2 = [storage imagePathWithSnapshotID:new_snapshot_id2];

  // Remove all images and ensure the files are removed.
  [storage removeAllImages];
  FlushRunLoops(storage);

  EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:[url1 path]]);
  EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:[url2 path]]);
  [storage retrieveImageWithSnapshotID:new_snapshot_id1
                          snapshotKind:SnapshotKindColor
                            completion:^(UIImage* retrievedImage1) {
                              EXPECT_FALSE(retrievedImage1);
                            }];
  [storage retrieveImageWithSnapshotID:new_snapshot_id2
                          snapshotKind:SnapshotKindColor
                            completion:^(UIImage* retrievedImage2) {
                              EXPECT_FALSE(retrievedImage2);
                            }];
}

// Tests that observers are notified when a snapshot is storaged and removed.
TEST_P(SnapshotStorageTest, ObserversNotifiedOnSetAndRemoveImage) {
  ASSERT_TRUE(CreateSnapshotStorage());
  id<SnapshotStorage> storage = GetSnapshotStorage();

  FakeSnapshotStorageObserver* observer =
      [[FakeSnapshotStorageObserver alloc] init];
  [storage addObserver:observer];
  EXPECT_FALSE(observer.lastUpdatedID.snapshot_id.valid());
  ASSERT_NE(test_images_.count, 0u);

  // Check if setting a new image is notified to the observer.
  SnapshotIDWrapper* snapshot_id = [[test_images_ keyEnumerator] nextObject];
  ASSERT_NSNE(snapshot_id, nil);

  [storage setImage:test_images_[snapshot_id] withSnapshotID:snapshot_id];
  EXPECT_NSEQ(snapshot_id, observer.lastUpdatedID);

  // Check if removing an image is notified to the observer.
  observer.lastUpdatedID = InvalidSnapshotID();
  [storage removeImageWithSnapshotID:snapshot_id];
  EXPECT_NSEQ(snapshot_id, observer.lastUpdatedID);
  [storage removeObserver:observer];
}

// Tests that creating the SnapshotStorage create the storage directory.
TEST_P(SnapshotStorageTest, CreateStorage) {
  ASSERT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_directory_.GetPath();

  const base::FilePath storage_path =
      root.Append(kSnapshotsDirName).Append(kIdentifier);

  NSURL* storage_url = base::apple::FilePathToNSURL(storage_path);
  id<SnapshotStorage> storage =
      [[SnapshotStorageImpl alloc] initWithStorageDirectoryUrl:storage_url];

  FlushRunLoops(storage);

  EXPECT_TRUE(base::DirectoryExists(storage_path));
}

// Tests that retrieving grey snapshot images generated by color images stored
// in cache.
TEST_P(SnapshotStorageTest, RetrieveGreyImageFromColorImageInMemory) {
  ASSERT_TRUE(CreateSnapshotStorage());
  LoadAllColorImagesIntoCache(true);

  id<SnapshotStorage> storage = GetSnapshotStorage();
  __block NSInteger numberOfCallbacks = 0;
  for (SnapshotIDWrapper* snapshot_id in test_images_) {
    [storage retrieveImageWithSnapshotID:snapshot_id
                            snapshotKind:SnapshotKindGreyscale
                              completion:^(UIImage* image) {
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
TEST_P(SnapshotStorageTest, RetrieveGreyImageFromColorImageInDisk) {
  ASSERT_TRUE(CreateSnapshotStorage());
  LoadAllColorImagesIntoCache(true);

  id<SnapshotStorage> storage = GetSnapshotStorage();

  // Remove color images from in-memory storage.
  ClearCache();

  __block NSInteger numberOfCallbacks = 0;
  for (SnapshotIDWrapper* snapshot_id in test_images_) {
    [storage retrieveImageWithSnapshotID:snapshot_id
                            snapshotKind:SnapshotKindGreyscale
                              completion:^(UIImage* image) {
                                EXPECT_TRUE(image);
                                ++numberOfCallbacks;
                              }];
  }

  // Wait until all callbacks are called.
  FlushRunLoops(storage);

  EXPECT_EQ(numberOfCallbacks, kSnapshotCount);
}
