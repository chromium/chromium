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

// A valid favicon file key (64 hex characters).
NSString* const kFaviconKey =
    @"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

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

  // Returns the file URL in the test favicon folder for `key`.
  NSURL* FileURLForKey(NSString* key) {
    return [base::apple::FilePathToNSURL(favicon_folder_.GetPath())
        URLByAppendingPathComponent:key
                        isDirectory:NO];
  }

  // Writes favicon attributes with `monogram` to `key` in the favicon folder.
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

  // Fetches `key` and waits for completion on the main thread. Returns the
  // fetched attributes.
  FaviconAttributes* FetchAndWait(NSString* key) {
    __block bool done = false;
    __block FaviconAttributes* result = nil;
    FetchFaviconAsync(key, ^(FaviconAttributes* attributes) {
      EXPECT_TRUE(NSThread.isMainThread);
      result = attributes;
      done = true;
    });
    // Completion must always be dispatched asynchronously rather than invoked
    // synchronously on the caller's stack.
    EXPECT_FALSE(done);
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForFileOperationTimeout, ^{
          return done;
        }));
    return result;
  }

 private:
  base::ScopedTempDir favicon_folder_;
};

// Tests that a favicon is read from the favicon folder and delivered on the
// main thread.
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

// Tests that unreadable or corrupt archive data results in nil and is not
// cached.
TEST_F(FaviconUtilTest, CorruptFaviconIsNotCached) {
  NSData* invalid_data =
      [@"not_an_archive" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE([invalid_data writeToURL:FileURLForKey(kFaviconKey)
                            atomically:YES]);

  EXPECT_FALSE(FetchAndWait(kFaviconKey));
  EXPECT_FALSE(CachedFaviconAttributes(kFaviconKey));
}

// Tests that nil and empty favicon keys result in nil.
TEST_F(FaviconUtilTest, EmptyOrNilKeyReturnsNil) {
  EXPECT_FALSE(FetchAndWait(nil));
  EXPECT_FALSE(FetchAndWait(@""));
  EXPECT_FALSE(CachedFaviconAttributes(nil));
  EXPECT_FALSE(CachedFaviconAttributes(@""));
}

// Tests that keys that aren't favicon file keys are not read, even if they
// resolve to an existing favicon file.
TEST_F(FaviconUtilTest, InvalidKeyIsNotRead) {
  ASSERT_NO_FATAL_FAILURE(WriteFavicon(kFaviconKey, @"A"));
  NSString* folder_name = FileURLForKey(kFaviconKey)
                              .URLByDeletingLastPathComponent.lastPathComponent;

  // A path escaping the favicon folder and resolving back to the file.
  NSString* traversal_key =
      [NSString stringWithFormat:@"../%@/%@", folder_name, kFaviconKey];
  EXPECT_FALSE(FetchAndWait(traversal_key));

  // An existing file whose name isn't a favicon file key.
  NSString* non_key = @"not_a_favicon_key";
  ASSERT_NO_FATAL_FAILURE(WriteFavicon(non_key, @"B"));
  EXPECT_FALSE(FetchAndWait(non_key));
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
