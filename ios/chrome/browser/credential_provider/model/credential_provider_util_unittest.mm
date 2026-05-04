// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"

#import "base/apple/foundation_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

NSString* const kTestFaviconKey = @"TEST";

class CredentialProviderUtilTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    SetFaviconsFolderURLForTesting(
        base::apple::FilePathToNSURL(scoped_temp_dir_.GetPath()));
  }

  void TearDown() override {
    SetFaviconsFolderURLForTesting(nil);
    PlatformTest::TearDown();
  }

  base::ScopedTempDir scoped_temp_dir_;
};

// Test that the expected hash stays the same for the same URL.
TEST_F(CredentialProviderUtilTest, GetFaviconFileKey) {
  EXPECT_NSEQ(
      GetFaviconFileKey(GURL("https://login.yahoo.com/")),
      @"BD7639C34EA3480A8AAD704306C8870161761506AD948AC6FA037B83CFF22D37");
  EXPECT_NSEQ(
      GetFaviconFileKey(GURL("www.kijiji.ca")),
      @"E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
  EXPECT_NSEQ(
      GetFaviconFileKey(
          GURL("https://www.theweathernetwork.com/ca/weather/quebec/montreal")),
      @"A0F3B5AB4012A2EC0EA3AC950B6AD8982F6FF29DE632ECC3645D566E291E3D12");
  EXPECT_NSEQ(
      GetFaviconFileKey(GURL(
          "https://www.canadapost-postescanada.ca/track-reperage/en#/home")),
      @"9DD8ED2F4B375E5DDDEA137D8985FFD32521694331753E70AA815692CFE0653B");
}

TEST_F(CredentialProviderUtilTest, ShouldFetchFavicon) {
  // Setup some dates for later.
  NSDate* today = [NSDate date];
  NSCalendar* calendar = [NSCalendar currentCalendar];
  NSDateComponents* offsetComponents = [[NSDateComponents alloc] init];
  [offsetComponents setDay:-13];
  NSDate* thirteenDaysAgo = [calendar dateByAddingComponents:offsetComponents
                                                      toDate:today
                                                     options:0];
  [offsetComponents setDay:-15];
  NSDate* fifteenDaysAgo = [calendar dateByAddingComponents:offsetComponents
                                                     toDate:today
                                                    options:0];

  EXPECT_TRUE(ShouldFetchFavicon(kTestFaviconKey, @{}));

  EXPECT_FALSE(ShouldFetchFavicon(kTestFaviconKey, @{kTestFaviconKey : today}));
  EXPECT_FALSE(ShouldFetchFavicon(kTestFaviconKey,
                                  @{kTestFaviconKey : thirteenDaysAgo}));
  EXPECT_TRUE(
      ShouldFetchFavicon(kTestFaviconKey, @{kTestFaviconKey : fifteenDaysAgo}));

  EXPECT_TRUE(ShouldFetchFavicon(kTestFaviconKey, @{@"OtherFavicon" : today}));

  // Edge cases around the 14-day boundary.
  base::Time now = base::Time::Now();
  NSDate* slightlyLessThanFourteenDaysAgo =
      (now - base::Days(14) + base::Seconds(5)).ToNSDate();
  NSDate* slightlyMoreThanFourteenDaysAgo =
      (now - base::Days(14) - base::Seconds(5)).ToNSDate();

  EXPECT_FALSE(ShouldFetchFavicon(
      kTestFaviconKey, @{kTestFaviconKey : slightlyLessThanFourteenDaysAgo}));
  EXPECT_TRUE(ShouldFetchFavicon(
      kTestFaviconKey, @{kTestFaviconKey : slightlyMoreThanFourteenDaysAgo}));

  // Date in the future should not trigger a fetch.
  NSDate* futureDate = (now + base::Days(1)).ToNSDate();
  EXPECT_FALSE(
      ShouldFetchFavicon(kTestFaviconKey, @{kTestFaviconKey : futureDate}));
}

TEST_F(CredentialProviderUtilTest, GetFaviconsListAndFreshness_NilFolder) {
  SetFaviconsFolderURLForTesting(nil);
  EXPECT_NSEQ(nil, GetFaviconsListAndFreshness());
}

TEST_F(CredentialProviderUtilTest,
       GetFaviconsListAndFreshness_NonExistentFolder) {
  NSURL* folder_url = [base::apple::FilePathToNSURL(scoped_temp_dir_.GetPath())
      URLByAppendingPathComponent:@"NonExistent"];
  SetFaviconsFolderURLForTesting(folder_url);
  EXPECT_NSEQ(nil, GetFaviconsListAndFreshness());
}

TEST_F(CredentialProviderUtilTest, GetFaviconsListAndFreshness_EmptyFolder) {
  EXPECT_NSEQ(nil, GetFaviconsListAndFreshness());
}

TEST_F(CredentialProviderUtilTest, GetFaviconsListAndFreshness_WithFiles) {
  NSURL* folder_url = base::apple::FilePathToNSURL(scoped_temp_dir_.GetPath());
  NSURL* file_url = [folder_url URLByAppendingPathComponent:@"file1"];

  NSError* error = nil;
  BOOL success = [@"dummy" writeToURL:file_url
                           atomically:YES
                             encoding:NSUTF8StringEncoding
                                error:&error];
  ASSERT_TRUE(success) << base::SysNSStringToUTF8([error description]);

  NSDictionary<NSString*, NSDate*>* dict = GetFaviconsListAndFreshness();
  ASSERT_NSNE(nil, dict);
  EXPECT_EQ(1u, dict.count);
  EXPECT_NSNE(nil, dict[@"file1"]);
}

}  // namespace
