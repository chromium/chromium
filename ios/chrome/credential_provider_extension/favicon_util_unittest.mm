// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/favicon_util.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

NSString* const kFaviconKey = @"favicon_key";

class FaviconUtilTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(favicon_folder_.CreateUniqueTempDir());
    SetFaviconFolderForTesting(
        base::apple::FilePathToNSURL(favicon_folder_.GetPath()));
  }

  void TearDown() override {
    SetFaviconFolderForTesting(nil);
    PlatformTest::TearDown();
  }

  NSURL* FileURLForKey(NSString* key) {
    return [base::apple::FilePathToNSURL(favicon_folder_.GetPath())
        URLByAppendingPathComponent:key
                        isDirectory:NO];
  }

  // Writes favicon attributes with `monogram` to the favicon folder.
  void WriteFavicon(NSString* key, NSString* monogram) {
    FaviconAttributes* attributes =
        [FaviconAttributes attributesWithMonogram:monogram
                                        textColor:UIColor.whiteColor
                                  backgroundColor:UIColor.blackColor
                           defaultBackgroundColor:NO];
    NSData* data = [NSKeyedArchiver archivedDataWithRootObject:attributes
                                         requiringSecureCoding:YES
                                                         error:nil];
    ASSERT_TRUE([data writeToURL:FileURLForKey(key) atomically:YES]);
  }

  // Fetches `key` and waits for the completion. Returns the fetched
  // attributes.
  FaviconAttributes* FetchAndWait(NSString* key) {
    __block bool done = false;
    __block FaviconAttributes* result = nil;
    FetchFaviconAsync(key, ^(FaviconAttributes* attributes) {
      dispatch_async(dispatch_get_main_queue(), ^{
        result = attributes;
        done = true;
      });
    });
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForFileOperationTimeout, ^{
          return done;
        }));
    return result;
  }

 private:
  base::ScopedTempDir favicon_folder_;
};

// Tests that a favicon is read from the favicon folder.
TEST_F(FaviconUtilTest, FetchReadsFaviconFromDisk) {
  ASSERT_NO_FATAL_FAILURE(WriteFavicon(kFaviconKey, @"A"));

  EXPECT_NSEQ(FetchAndWait(kFaviconKey).monogramString, @"A");
}

// Tests that a fetched favicon is cached and served without reading the disk
// again.
TEST_F(FaviconUtilTest, FetchedFaviconIsCached) {
  ASSERT_NO_FATAL_FAILURE(WriteFavicon(kFaviconKey, @"A"));
  ASSERT_FALSE(CachedFaviconAttributes(kFaviconKey));

  ASSERT_TRUE(FetchAndWait(kFaviconKey));
  EXPECT_NSEQ(CachedFaviconAttributes(kFaviconKey).monogramString, @"A");

  ASSERT_TRUE([[NSFileManager defaultManager]
      removeItemAtURL:FileURLForKey(kFaviconKey)
                error:nil]);
  EXPECT_NSEQ(FetchAndWait(kFaviconKey).monogramString, @"A");
}

// Tests that a missing favicon results in nil and is not cached.
TEST_F(FaviconUtilTest, MissingFaviconIsNotCached) {
  EXPECT_FALSE(FetchAndWait(kFaviconKey));
  EXPECT_FALSE(CachedFaviconAttributes(kFaviconKey));

  // A favicon written later is picked up by the next fetch.
  ASSERT_NO_FATAL_FAILURE(WriteFavicon(kFaviconKey, @"B"));
  EXPECT_NSEQ(FetchAndWait(kFaviconKey).monogramString, @"B");
}

// Tests that an empty favicon key results in nil.
TEST_F(FaviconUtilTest, EmptyKeyReturnsNil) {
  EXPECT_FALSE(FetchAndWait(@""));
  EXPECT_FALSE(CachedFaviconAttributes(@""));
}

// Tests that resetting the favicon folder clears the cache.
TEST_F(FaviconUtilTest, ChangingFolderClearsCache) {
  ASSERT_NO_FATAL_FAILURE(WriteFavicon(kFaviconKey, @"A"));
  ASSERT_TRUE(FetchAndWait(kFaviconKey));

  base::ScopedTempDir other_folder;
  ASSERT_TRUE(other_folder.CreateUniqueTempDir());
  SetFaviconFolderForTesting(
      base::apple::FilePathToNSURL(other_folder.GetPath()));

  EXPECT_FALSE(CachedFaviconAttributes(kFaviconKey));
  EXPECT_FALSE(FetchAndWait(kFaviconKey));
}

}  // namespace
