// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_cache.h"

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_internal.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_observer.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

static const NSUInteger kSessionCount = 10;
static const NSUInteger kSnapshotPixelSize = 8;

@interface FakeSnapshotCacheObserver : NSObject<SnapshotCacheObserver>
@property(nonatomic, copy) NSString* lastUpdatedIdentifier;
@end

@implementation FakeSnapshotCacheObserver
@synthesize lastUpdatedIdentifier = _lastUpdatedIdentifier;
- (void)snapshotCache:(SnapshotCache*)snapshotCache
    didUpdateSnapshotForIdentifier:(NSString*)identifier {
  self.lastUpdatedIdentifier = identifier;
}
@end

namespace {

class SnapshotCacheTest : public PlatformTest {
 protected:
  // Build an array of session names and an array of UIImages filled with
  // random colors.
  void SetUp() override {
    PlatformTest::SetUp();
    snapshotCache_ = [[SnapshotCache alloc] init];
    testImages_ = [[NSMutableArray alloc] initWithCapacity:kSessionCount];
    testSessions_ = [[NSMutableArray alloc] initWithCapacity:kSessionCount];

    CGFloat scale = [snapshotCache_ snapshotScaleForDevice];
    UIGraphicsBeginImageContextWithOptions(
        CGSizeMake(kSnapshotPixelSize, kSnapshotPixelSize), NO, scale);
    CGContextRef context = UIGraphicsGetCurrentContext();
    srand(1);

    for (NSUInteger i = 0; i < kSessionCount; ++i) {
      UIImage* image = GenerateRandomImage(context);
      [testImages_ addObject:image];
      [testSessions_
          addObject:[NSString stringWithFormat:@"SessionId-%" PRIuNS, i]];
    }

    UIGraphicsEndImageContext();

    ClearDumpedImages();
  }

  void TearDown() override {
    ClearDumpedImages();
    [snapshotCache_ shutdown];
    snapshotCache_ = nil;
    PlatformTest::TearDown();
  }

  SnapshotCache* GetSnapshotCache() { return snapshotCache_; }

  // Generates an image filled with a random color.
  UIImage* GenerateRandomImage(CGContextRef context) {
    CGFloat r = rand() / CGFloat(RAND_MAX);
    CGFloat g = rand() / CGFloat(RAND_MAX);
    CGFloat b = rand() / CGFloat(RAND_MAX);
    CGContextSetRGBStrokeColor(context, r, g, b, 1.0);
    CGContextSetRGBFillColor(context, r, g, b, 1.0);
    CGContextFillRect(
        context, CGRectMake(0.0, 0.0, kSnapshotPixelSize, kSnapshotPixelSize));
    return UIGraphicsGetImageFromCurrentImageContext();
  }

  // Generates an image of |size|, filled with a random color.
  UIImage* GenerateRandomImage(CGSize size) {
    UIGraphicsBeginImageContextWithOptions(size, /*opaque=*/NO,
                                           UIScreen.mainScreen.scale);
    CGContextRef context = UIGraphicsGetCurrentContext();
    UIImage* image = GenerateRandomImage(context);
    UIGraphicsEndImageContext();
    return image;
  }

  // Flushes all the runloops internally used by the snapshot cache.
  void FlushRunLoops() {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  // This function removes the snapshots both from dictionary and from disk.
  void ClearDumpedImages() {
    SnapshotCache* cache = GetSnapshotCache();

    NSString* sessionID;
    for (sessionID in testSessions_)
      [cache removeImageWithSessionID:sessionID];

    FlushRunLoops();
    // The above calls to -removeImageWithSessionID remove both the color
    // and grey snapshots for each sessionID, if they are on disk.  However,
    // ensure we also get rid of the grey snapshots in memory.
    [cache removeGreyCache];

    __block BOOL foundImage = NO;
    __block NSUInteger numCallbacks = 0;
    for (sessionID in testSessions_) {
      base::FilePath path([cache imagePathForSessionID:sessionID]);

      // Checks that the snapshot is not on disk.
      EXPECT_FALSE(base::PathExists(path));

      // Check that the snapshot is not in the dictionary.
      [cache retrieveImageForSessionID:sessionID
                              callback:^(UIImage* image) {
                                ++numCallbacks;
                                if (image)
                                  foundImage = YES;
                              }];
    }

    // Expect that all the callbacks ran and that none retrieved an image.
    FlushRunLoops();
    EXPECT_EQ([testSessions_ count], numCallbacks);
    EXPECT_FALSE(foundImage);
  }

  // Loads kSessionCount color images into the cache.  If |waitForFilesOnDisk|
  // is YES, will not return until the images have been written to disk.
  void LoadAllColorImagesIntoCache(bool waitForFilesOnDisk) {
    LoadColorImagesIntoCache(kSessionCount, waitForFilesOnDisk);
  }

  // Loads |count| color images into the cache.  If |waitForFilesOnDisk|
  // is YES, will not return until the images have been written to disk.
  void LoadColorImagesIntoCache(NSUInteger count, bool waitForFilesOnDisk) {
    SnapshotCache* cache = GetSnapshotCache();
    // Put color images in the cache.
    for (NSUInteger i = 0; i < count; ++i) {
      @autoreleasepool {
        UIImage* image = [testImages_ objectAtIndex:i];
        NSString* sessionID = [testSessions_ objectAtIndex:i];
        [cache setImage:image withSessionID:sessionID];
      }
    }
    if (waitForFilesOnDisk) {
      FlushRunLoops();
      for (NSUInteger i = 0; i < count; ++i) {
        // Check that images are on the disk.
        NSString* sessionID = [testSessions_ objectAtIndex:i];
        base::FilePath path([cache imagePathForSessionID:sessionID]);
        EXPECT_TRUE(base::PathExists(path));
      }
    }
  }

  // Waits for the first |count| grey images for sessions in |testSessions_|
  // to be placed in the cache.
  void WaitForGreyImagesInCache(NSUInteger count) {
    SnapshotCache* cache = GetSnapshotCache();
    FlushRunLoops();
    for (NSUInteger i = 0; i < count; i++)
      EXPECT_TRUE([cache hasGreyImageInMemory:testSessions_[i]]);
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
  SnapshotCache* snapshotCache_;
  NSMutableArray* testSessions_;
  NSMutableArray* testImages_;
};

// This test simply put all the snapshots in the cache and then gets them back
// As the snapshots are kept in memory, the same pointer can be retrieved.
// This test also checks that images are correctly removed from the disk.
TEST_F(SnapshotCacheTest, Cache) {
  SnapshotCache* cache = GetSnapshotCache();

  NSUInteger expectedCacheSize = MIN(kSessionCount, [cache lruCacheMaxSize]);

  // Put all images in the cache.
  for (NSUInteger i = 0; i < expectedCacheSize; ++i) {
    UIImage* image = [testImages_ objectAtIndex:i];
    NSString* sessionID = [testSessions_ objectAtIndex:i];
    [cache setImage:image withSessionID:sessionID];
  }

  // Get images back.
  __block NSUInteger numberOfCallbacks = 0;
  for (NSUInteger i = 0; i < expectedCacheSize; ++i) {
    NSString* sessionID = [testSessions_ objectAtIndex:i];
    UIImage* expectedImage = [testImages_ objectAtIndex:i];
    EXPECT_TRUE(expectedImage != nil);
    [cache retrieveImageForSessionID:sessionID
                            callback:^(UIImage* image) {
                              // Images have not been removed from the
                              // dictionnary. We expect the same pointer.
                              EXPECT_EQ(expectedImage, image);
                              ++numberOfCallbacks;
                            }];
  }
  EXPECT_EQ(expectedCacheSize, numberOfCallbacks);
}

// This test puts all the snapshots in the cache and flushes them to disk.
// The snapshots are then reloaded from the disk, and the colors are compared.
TEST_F(SnapshotCacheTest, SaveToDisk) {
  SnapshotCache* cache = GetSnapshotCache();

  // Put all images in the cache.
  for (NSUInteger i = 0; i < kSessionCount; ++i) {
    UIImage* image = [testImages_ objectAtIndex:i];
    NSString* sessionID = [testSessions_ objectAtIndex:i];
    [cache setImage:image withSessionID:sessionID];
  }
  FlushRunLoops();

  for (NSUInteger i = 0; i < kSessionCount; ++i) {
    // Check that images are on the disk.
    NSString* sessionID = [testSessions_ objectAtIndex:i];

    base::FilePath path([cache imagePathForSessionID:sessionID]);
    EXPECT_TRUE(base::PathExists(path));

    // Check image colors by comparing the first pixel against the reference
    // image.
    UIImage* image =
        [UIImage imageWithContentsOfFile:base::SysUTF8ToNSString(path.value())];
    CGImageRef cgImage = [image CGImage];
    ASSERT_TRUE(cgImage != nullptr);

    base::ScopedCFTypeRef<CFDataRef> pixelData(
        CGDataProviderCopyData(CGImageGetDataProvider(cgImage)));
    const char* pixels =
        reinterpret_cast<const char*>(CFDataGetBytePtr(pixelData));
    EXPECT_TRUE(pixels);

    UIImage* referenceImage = [testImages_ objectAtIndex:i];
    CGImageRef referenceCgImage = [referenceImage CGImage];
    base::ScopedCFTypeRef<CFDataRef> referenceData(
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

TEST_F(SnapshotCacheTest, Purge) {
  SnapshotCache* cache = GetSnapshotCache();

  // Put all images in the cache.
  for (NSUInteger i = 0; i < kSessionCount; ++i) {
    UIImage* image = [testImages_ objectAtIndex:i];
    NSString* sessionID = [testSessions_ objectAtIndex:i];
    [cache setImage:image withSessionID:sessionID];
  }

  NSMutableSet* liveSessions = [NSMutableSet setWithCapacity:1];
  [liveSessions addObject:[testSessions_ objectAtIndex:0]];

  // Purge the cache.
  [cache purgeCacheOlderThan:(base::Time::Now() - base::TimeDelta::FromHours(1))
                     keeping:liveSessions];
  FlushRunLoops();

  // Check that nothing has been deleted.
  for (NSUInteger i = 0; i < kSessionCount; ++i) {
    // Check that images are on the disk.
    NSString* sessionID = [testSessions_ objectAtIndex:i];

    base::FilePath path([cache imagePathForSessionID:sessionID]);
    EXPECT_TRUE(base::PathExists(path));
  }

  // Purge the cache.
  [cache purgeCacheOlderThan:base::Time::Now() keeping:liveSessions];
  FlushRunLoops();

  // Check that the file have been deleted.
  for (NSUInteger i = 0; i < kSessionCount; ++i) {
    // Check that images are on the disk.
    NSString* sessionID = [testSessions_ objectAtIndex:i];

    base::FilePath path([cache imagePathForSessionID:sessionID]);
    if (i == 0)
      EXPECT_TRUE(base::PathExists(path));
    else
      EXPECT_FALSE(base::PathExists(path));
  }
}

// Loads the color images into the cache, and pins two of them.  Ensures that
// only the two pinned IDs remain in memory after a memory warning.
TEST_F(SnapshotCacheTest, HandleMemoryWarning) {
  LoadAllColorImagesIntoCache(true);

  SnapshotCache* cache = GetSnapshotCache();

  NSString* firstPinnedID = [testSessions_ objectAtIndex:4];
  NSString* secondPinnedID = [testSessions_ objectAtIndex:6];
  NSMutableSet* set = [NSMutableSet set];
  [set addObject:firstPinnedID];
  [set addObject:secondPinnedID];
  cache.pinnedIDs = set;

  TriggerMemoryWarning();

  EXPECT_EQ(YES, [cache hasImageInMemory:firstPinnedID]);
  EXPECT_EQ(YES, [cache hasImageInMemory:secondPinnedID]);

  NSString* notPinnedID = [testSessions_ objectAtIndex:2];
  EXPECT_FALSE([cache hasImageInMemory:notPinnedID]);

  // Wait for the final image to be pulled off disk.
  FlushRunLoops();
}

// Tests that createGreyCache creates the grey snapshots in the background,
// from color images in the in-memory cache.  When the grey images are all
// loaded into memory, tests that the request to retrieve the grey snapshot
// calls the callback immediately.
// Disabled on simulators because it sometimes crashes. crbug/421425
#if !TARGET_IPHONE_SIMULATOR
TEST_F(SnapshotCacheTest, CreateGreyCache) {
  LoadAllColorImagesIntoCache(true);

  // Request the creation of a grey image cache for all images.
  SnapshotCache* cache = GetSnapshotCache();
  [cache createGreyCache:testSessions_];

  // Wait for them to be put into the grey image cache.
  WaitForGreyImagesInCache(kSessionCount);

  __block NSUInteger numberOfCallbacks = 0;
  for (NSUInteger i = 0; i < kSessionCount; ++i) {
    NSString* sessionID = [testSessions_ objectAtIndex:i];
    [cache retrieveGreyImageForSessionID:sessionID
                                callback:^(UIImage* image) {
                                  EXPECT_TRUE(image);
                                  ++numberOfCallbacks;
                                }];
  }

  EXPECT_EQ(numberOfCallbacks, kSessionCount);
}

// Same as previous test, except that all the color images are on disk,
// rather than in memory.
// Disabled due to the greyImage crash.  b/8048597
TEST_F(SnapshotCacheTest, CreateGreyCacheFromDisk) {
  LoadAllColorImagesIntoCache(true);

  // Remove color images from in-memory cache.
  SnapshotCache* cache = GetSnapshotCache();

  TriggerMemoryWarning();

  // Request the creation of a grey image cache for all images.
  [cache createGreyCache:testSessions_];

  // Wait for them to be put into the grey image cache.
  WaitForGreyImagesInCache(kSessionCount);

  __block NSUInteger numberOfCallbacks = 0;
  for (NSUInteger i = 0; i < kSessionCount; ++i) {
    NSString* sessionID = [testSessions_ objectAtIndex:i];
    [cache retrieveGreyImageForSessionID:sessionID
                                callback:^(UIImage* image) {
                                  EXPECT_TRUE(image);
                                  ++numberOfCallbacks;
                                }];
  }

  EXPECT_EQ(numberOfCallbacks, kSessionCount);
}
#endif  // !TARGET_IPHONE_SIMULATOR

// Tests mostRecentGreyBlock, which is a block to be called when the most
// recently requested grey image is finally loaded.
// The test requests three images be cached as grey images.  Only the final
// callback of the three requests should be called.
// Disabled due to the greyImage crash.  b/8048597
TEST_F(SnapshotCacheTest, MostRecentGreyBlock) {
  const NSUInteger kNumImages = 3;
  NSMutableArray* sessionIDs =
      [[NSMutableArray alloc] initWithCapacity:kNumImages];
  [sessionIDs addObject:[testSessions_ objectAtIndex:0]];
  [sessionIDs addObject:[testSessions_ objectAtIndex:1]];
  [sessionIDs addObject:[testSessions_ objectAtIndex:2]];

  SnapshotCache* cache = GetSnapshotCache();

  // Put 3 images in the cache.
  LoadColorImagesIntoCache(kNumImages, true);
  // Make sure the color images are only on disk, to ensure the background
  // thread is slow enough to queue up the requests.
  TriggerMemoryWarning();

  // Enable the grey image cache.
  [cache createGreyCache:sessionIDs];

  // Request the grey versions
  __block BOOL firstCallbackCalled = NO;
  __block BOOL secondCallbackCalled = NO;
  __block BOOL thirdCallbackCalled = NO;
  [cache greyImageForSessionID:[testSessions_ objectAtIndex:0]
                      callback:^(UIImage*) {
                        firstCallbackCalled = YES;
                      }];
  [cache greyImageForSessionID:[testSessions_ objectAtIndex:1]
                      callback:^(UIImage*) {
                        secondCallbackCalled = YES;
                      }];
  [cache greyImageForSessionID:[testSessions_ objectAtIndex:2]
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
TEST_F(SnapshotCacheTest, GreyImageAllInBackground) {
  LoadAllColorImagesIntoCache(true);

  SnapshotCache* cache = GetSnapshotCache();

  // Now convert every image into a grey image, on disk, in the background.
  for (NSUInteger i = 0; i < kSessionCount; ++i) {
    [cache saveGreyInBackgroundForSessionID:[testSessions_ objectAtIndex:i]];
  }

  // Waits for the grey images for the sessions in |testSessions_| to be written
  // to disk, which happens in a background thread.
  FlushRunLoops();

  for (NSString* sessionID in testSessions_) {
    base::FilePath path([cache greyImagePathForSessionID:sessionID]);
    EXPECT_TRUE(base::PathExists(path));
    base::DeleteFile(path, false);
  }
}

// Verifies that image size and scale are preserved when writing and reading
// from disk.
TEST_F(SnapshotCacheTest, SizeAndScalePreservation) {
  SnapshotCache* cache = GetSnapshotCache();

  // Create an image with the expected snapshot scale.
  CGFloat scale = [cache snapshotScaleForDevice];
  UIGraphicsBeginImageContextWithOptions(
      CGSizeMake(kSnapshotPixelSize, kSnapshotPixelSize), NO, scale);
  CGContextRef context = UIGraphicsGetCurrentContext();
  UIImage* image = GenerateRandomImage(context);
  UIGraphicsEndImageContext();

  // Add the image to the cache then call handle low memory to ensure the image
  // is read from disk instead of the in-memory cache.
  NSString* const kSession = @"foo";
  [cache setImage:image withSessionID:kSession];
  FlushRunLoops();  // ensure the file is written to disk.
  TriggerMemoryWarning();

  // Retrive the image and have the callback verify the size and scale.
  __block BOOL callbackComplete = NO;
  [cache retrieveImageForSessionID:kSession
                          callback:^(UIImage* imageFromDisk) {
                            EXPECT_EQ(image.size.width,
                                      imageFromDisk.size.width);
                            EXPECT_EQ(image.size.height,
                                      imageFromDisk.size.height);
                            EXPECT_EQ(image.scale, imageFromDisk.scale);
                            callbackComplete = YES;
                          }];
  FlushRunLoops();
  EXPECT_TRUE(callbackComplete);
}

// Verifies that retina-scale images are deleted properly.
TEST_F(SnapshotCacheTest, DeleteRetinaImages) {
  SnapshotCache* cache = GetSnapshotCache();
  if ([cache snapshotScaleForDevice] != 2.0) {
    return;
  }

  // Create an image with retina scale.
  UIGraphicsBeginImageContextWithOptions(
      CGSizeMake(kSnapshotPixelSize, kSnapshotPixelSize), NO, 2.0);
  CGContextRef context = UIGraphicsGetCurrentContext();
  UIImage* image = GenerateRandomImage(context);
  UIGraphicsEndImageContext();

  // Add the image to the cache then call handle low memory to ensure the image
  // is read from disk instead of the in-memory cache.
  NSString* const kSession = @"foo";
  [cache setImage:image withSessionID:kSession];
  FlushRunLoops();  // ensure the file is written to disk.
  TriggerMemoryWarning();

  // Verify the file was writted with @2x in the file name.
  base::FilePath retinaFile = [cache imagePathForSessionID:kSession];
  EXPECT_TRUE(base::PathExists(retinaFile));

  // Delete the image.
  [cache removeImageWithSessionID:kSession];
  FlushRunLoops();  // ensure the file is removed.

  EXPECT_FALSE(base::PathExists(retinaFile));
}

// Tests that a marked image does not immediately delete when calling
// |-removeImageWithSessionID:|. Calling |-removeMarkedImages| immediately
// deletes the marked image.
TEST_F(SnapshotCacheTest, MarkedImageNotImmediatelyDeleted) {
  SnapshotCache* cache = GetSnapshotCache();
  UIImage* image =
      GenerateRandomImage(CGSizeMake(kSnapshotPixelSize, kSnapshotPixelSize));
  [cache setImage:image withSessionID:@"sessionID"];
  base::FilePath image_path = [cache imagePathForSessionID:@"sessionID"];
  [cache markImageWithSessionID:@"sessionID"];
  [cache removeImageWithSessionID:@"sessionID"];
  // Give enough time for deletion.
  FlushRunLoops();
  EXPECT_TRUE(base::PathExists(image_path));
  [cache removeMarkedImages];
  FlushRunLoops();
  EXPECT_FALSE(base::PathExists(image_path));
}

// Tests that unmarked images are not deleted when calling
// |-removeMarkedImages|.
TEST_F(SnapshotCacheTest, UnmarkedImageNotDeleted) {
  SnapshotCache* cache = GetSnapshotCache();
  UIImage* image =
      GenerateRandomImage(CGSizeMake(kSnapshotPixelSize, kSnapshotPixelSize));
  [cache setImage:image withSessionID:@"sessionID"];
  base::FilePath image_path = [cache imagePathForSessionID:@"sessionID"];
  [cache markImageWithSessionID:@"sessionID"];
  [cache unmarkAllImages];
  [cache removeMarkedImages];
  // Give enough time for deletion.
  FlushRunLoops();
  EXPECT_TRUE(base::PathExists(image_path));
}

// Tests that observers are notified when a snapshot is cached and removed.
TEST_F(SnapshotCacheTest, ObserversNotifiedOnSetAndRemoveImage) {
  SnapshotCache* cache = GetSnapshotCache();
  FakeSnapshotCacheObserver* observer =
      [[FakeSnapshotCacheObserver alloc] init];
  [cache addObserver:observer];
  EXPECT_NSEQ(nil, observer.lastUpdatedIdentifier);
  UIImage* image = [testImages_ objectAtIndex:0];
  NSString* sessionID = [testSessions_ objectAtIndex:0];
  [cache setImage:image withSessionID:sessionID];
  EXPECT_NSEQ(sessionID, observer.lastUpdatedIdentifier);
  observer.lastUpdatedIdentifier = nil;
  [cache removeImageWithSessionID:sessionID];
  EXPECT_NSEQ(sessionID, observer.lastUpdatedIdentifier);
  [cache removeObserver:observer];
}
}  // namespace
