// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/ntp_tile_saver.h"

#include "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#import "components/ntp_tiles/ntp_tile.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/favicon/favicon_attributes.h"
#import "ios/chrome/common/ntp_tile/ntp_tile.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class NTPTileSaverControllerTest : public BlockCleanupTest {
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();
    if ([[NSFileManager defaultManager]
            fileExistsAtPath:[TestFaviconDirectory() path]]) {
      [[NSFileManager defaultManager] removeItemAtURL:TestFaviconDirectory()
                                                error:nil];
    }
    CreateMockImage([UIColor blackColor]);
    NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
    [sharedDefaults removeObjectForKey:app_group::kSuggestedItems];
  }

  void TearDown() override {
    if ([[NSFileManager defaultManager]
            fileExistsAtPath:[TestFaviconDirectory() path]]) {
      [[NSFileManager defaultManager] removeItemAtURL:TestFaviconDirectory()
                                                error:nil];
    }
    NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
    [sharedDefaults removeObjectForKey:app_group::kSuggestedItems];
    BlockCleanupTest::TearDown();
  }

  UIImage* CreateMockImage(UIColor* color) {
    mock_image_ = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(10, 10), color);
    return mock_image_;
  }

  NSURL* TestFaviconDirectory() {
    return
        [[[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                                 inDomains:NSUserDomainMask]
            firstObject] URLByAppendingPathComponent:@"testFaviconDirectory"];
  }

  void SetupMockCallback(id mock,
                         std::set<GURL> image_urls,
                         std::set<GURL> fallback_urls) {
    OCMStub([[mock ignoringNonObjectArgs]
                fetchFaviconAttributesForURL:GURL()
                                  completion:[OCMArg isNotNil]])
        .andDo(^(NSInvocation* invocation) {
          GURL* urltest;
          [invocation getArgument:&urltest atIndex:2];
          if (image_urls.find(GURL(*urltest)) != image_urls.end()) {
            __unsafe_unretained void (^callback)(id);
            [invocation getArgument:&callback atIndex:3];
            callback([FaviconAttributes attributesWithImage:mock_image_]);
          } else if (fallback_urls.find(GURL(*urltest)) !=
                     fallback_urls.end()) {
            __unsafe_unretained void (^callback)(id);
            [invocation getArgument:&callback atIndex:3];
            callback([FaviconAttributes
                attributesWithMonogram:@"C"
                             textColor:UIColor.whiteColor
                       backgroundColor:UIColor.blueColor
                defaultBackgroundColor:NO]);
          }
        });
  }

  // Checks that |tile| has an image and no fallback data.
  // Checks that |tile| title and url match |expected_title| and
  // |expected_url|.
  // Checks that the file pointed by |tile| match |mock_image_|.
  void VerifyWithImage(NTPTile* tile,
                       NSString* expected_title,
                       NSURL* expected_url) {
    EXPECT_NSNE(tile, nil);
    EXPECT_NSEQ(tile.title, expected_title);
    EXPECT_NSEQ(tile.URL, expected_url);
    EXPECT_NSNE(tile.faviconFileName, nil);
    EXPECT_NSEQ(tile.fallbackTextColor, nil);
    EXPECT_NSEQ(tile.fallbackBackgroundColor, nil);
    EXPECT_TRUE([[NSFileManager defaultManager]
        fileExistsAtPath:[[TestFaviconDirectory()
                             URLByAppendingPathComponent:tile.faviconFileName]
                             path]]);
    UIImage* stored_image = [UIImage
        imageWithContentsOfFile:
            [[TestFaviconDirectory()
                URLByAppendingPathComponent:tile.faviconFileName] path]];
    EXPECT_NSEQ(UIImagePNGRepresentation(stored_image),
                UIImagePNGRepresentation(mock_image_));
  }

  // Checks that |tile| has fallback data.
  // Checks that |tile| title and url match |expected_title| and
  // |expected_url|.
  // Checks that the file pointed by |tile| does not exist.
  void VerifyWithFallback(NTPTile* tile,
                          NSString* expected_title,
                          NSURL* expected_url) {
    EXPECT_NSNE(tile, nil);
    EXPECT_NSEQ(tile.title, expected_title);
    EXPECT_NSEQ(tile.URL, expected_url);
    EXPECT_NSNE(tile.faviconFileName, nil);
    EXPECT_NSEQ(tile.fallbackTextColor, UIColor.whiteColor);
    EXPECT_NSEQ(tile.fallbackBackgroundColor, UIColor.blueColor);
    EXPECT_EQ(tile.fallbackIsDefaultColor, NO);
    EXPECT_FALSE([[NSFileManager defaultManager]
        fileExistsAtPath:[[TestFaviconDirectory()
                             URLByAppendingPathComponent:tile.faviconFileName]
                             path]]);
  }

  // Checks that |tile| has an image and fallback data.
  // Checks that |tile| title and url match |expected_title| and
  // |expected_url|.
  // Checks that the file pointed by |tile| match |mock_image_|.
  void VerifyWithFallbackAndImage(NTPTile* tile,
                                  NSString* expected_title,
                                  NSURL* expected_url) {
    EXPECT_NSNE(tile, nil);
    EXPECT_NSEQ(tile.title, expected_title);
    EXPECT_NSEQ(tile.URL, expected_url);
    EXPECT_NSNE(tile.faviconFileName, nil);
    EXPECT_NSEQ(tile.fallbackTextColor, UIColor.whiteColor);
    EXPECT_NSEQ(tile.fallbackBackgroundColor, UIColor.blueColor);
    EXPECT_EQ(tile.fallbackIsDefaultColor, NO);
    EXPECT_TRUE([[NSFileManager defaultManager]
        fileExistsAtPath:[[TestFaviconDirectory()
                             URLByAppendingPathComponent:tile.faviconFileName]
                             path]]);
    UIImage* stored_image = [UIImage
        imageWithContentsOfFile:
            [[TestFaviconDirectory()
                URLByAppendingPathComponent:tile.faviconFileName] path]];
    EXPECT_NSEQ(UIImagePNGRepresentation(stored_image),
                UIImagePNGRepresentation(mock_image_));
  }

 protected:
  base::test::TaskEnvironment scoped_task_evironment_;
  UIImage* mock_image_;
};

TEST_F(NTPTileSaverControllerTest, SaveMostVisitedToDisk) {
  ntp_tiles::NTPTile image_tile = ntp_tiles::NTPTile();
  image_tile.title = base::ASCIIToUTF16("Title");
  image_tile.url = GURL("http://image.com");

  ntp_tiles::NTPTile fallback_tile = ntp_tiles::NTPTile();
  fallback_tile.title = base::ASCIIToUTF16("Title");
  fallback_tile.url = GURL("http://fallback.com");

  id mock_favicon_fetcher = OCMClassMock([FaviconAttributesProvider class]);
  SetupMockCallback(mock_favicon_fetcher, {image_tile.url},
                    {fallback_tile.url});

  ntp_tiles::NTPTilesVector tiles = {
      fallback_tile,  // NTP tile with fallback data
      image_tile,     // NTP tile with favicon
  };

  ntp_tile_saver::SaveMostVisitedToDisk(tiles, mock_favicon_fetcher,
                                        TestFaviconDirectory());
  // Wait for all asynchronous tasks to complete.
  scoped_task_evironment_.RunUntilIdle();

  // Read most visited from disk.
  NSDictionary<NSURL*, NTPTile*>* saved_tiles =
      ntp_tile_saver::ReadSavedMostVisited();

  EXPECT_EQ(saved_tiles.count, 2U);

  NSString* fallback_title = base::SysUTF16ToNSString(fallback_tile.title);
  NSURL* fallback_url = net::NSURLWithGURL(fallback_tile.url);
  NTPTile* fallback_saved_tile = [saved_tiles objectForKey:fallback_url];
  VerifyWithFallback(fallback_saved_tile, fallback_title, fallback_url);

  NSString* image_title = base::SysUTF16ToNSString(image_tile.title);
  NSURL* image_url = net::NSURLWithGURL(image_tile.url);
  NTPTile* image_saved_tile = [saved_tiles objectForKey:image_url];
  VerifyWithImage(image_saved_tile, image_title, image_url);
}

TEST_F(NTPTileSaverControllerTest, UpdateSingleFaviconFallback) {
  // Set up test with 3 saved sites, 2 of which have a favicon.
  ntp_tiles::NTPTile image_tile1 = ntp_tiles::NTPTile();
  image_tile1.title = base::ASCIIToUTF16("Title1");
  image_tile1.url = GURL("http://image1.com");

  ntp_tiles::NTPTile image_tile2 = ntp_tiles::NTPTile();
  image_tile2.title = base::ASCIIToUTF16("Title2");
  image_tile2.url = GURL("http://image2.com");

  ntp_tiles::NTPTile fallback_tile = ntp_tiles::NTPTile();
  fallback_tile.title = base::ASCIIToUTF16("Title");
  fallback_tile.url = GURL("http://fallback.com");

  id mock_favicon_fetcher = OCMClassMock([FaviconAttributesProvider class]);
  SetupMockCallback(mock_favicon_fetcher, {image_tile1.url, image_tile2.url},
                    {fallback_tile.url});

  ntp_tiles::NTPTilesVector tiles = {image_tile1, fallback_tile, image_tile2};

  ntp_tile_saver::SaveMostVisitedToDisk(tiles, mock_favicon_fetcher,
                                        TestFaviconDirectory());
  // Wait for all asynchronous tasks to complete.
  scoped_task_evironment_.RunUntilIdle();

  // Read most visited from disk.
  NSDictionary<NSURL*, NTPTile*>* saved_tiles =
      ntp_tile_saver::ReadSavedMostVisited();

  EXPECT_EQ(saved_tiles.count, 3U);

  NSString* image_title1 = base::SysUTF16ToNSString(image_tile1.title);
  NSURL* image_url1 = net::NSURLWithGURL(image_tile1.url);
  NTPTile* image_saved_tile1 = [saved_tiles objectForKey:image_url1];
  VerifyWithImage(image_saved_tile1, image_title1, image_url1);

  NSString* image_title2 = base::SysUTF16ToNSString(image_tile2.title);
  NSURL* image_url2 = net::NSURLWithGURL(image_tile2.url);
  NTPTile* image_saved_tile2 = [saved_tiles objectForKey:image_url2];
  VerifyWithImage(image_saved_tile2, image_title2, image_url2);

  NSString* fallback_title = base::SysUTF16ToNSString(fallback_tile.title);
  NSURL* fallback_url = net::NSURLWithGURL(fallback_tile.url);
  NTPTile* fallback_saved_tile = [saved_tiles objectForKey:fallback_url];
  VerifyWithFallback(fallback_saved_tile, fallback_title, fallback_url);

  // Mock returning a fallback value for the first image tile.
  id mock_favicon_fetcher2 = OCMClassMock([FaviconAttributesProvider class]);
  SetupMockCallback(mock_favicon_fetcher2, {image_tile2.url},
                    {image_tile1.url, fallback_tile.url});
  ntp_tile_saver::UpdateSingleFavicon(image_tile1.url, mock_favicon_fetcher2,
                                      TestFaviconDirectory());
  // Wait for all asynchronous tasks to complete.
  scoped_task_evironment_.RunUntilIdle();

  // Read most visited from disk.
  NSDictionary<NSURL*, NTPTile*>* saved_tiles_after_update =
      ntp_tile_saver::ReadSavedMostVisited();

  EXPECT_EQ(saved_tiles_after_update.count, 3U);

  // Verify that the first image tile now has callback data.
  image_saved_tile1 = [saved_tiles_after_update objectForKey:image_url1];
  VerifyWithFallback(image_saved_tile1, image_title1, image_url1);

  // Verify that the other two tiles did not change.
  image_saved_tile2 = [saved_tiles objectForKey:image_url2];
  VerifyWithImage(image_saved_tile2, image_title2, image_url2);
  fallback_saved_tile = [saved_tiles_after_update objectForKey:fallback_url];
  VerifyWithFallback(fallback_saved_tile, fallback_title, fallback_url);
}

// Checks that the image saved for an item is deleted when the item is deleted.
TEST_F(NTPTileSaverControllerTest, DeleteOutdatedImage) {
  ntp_tiles::NTPTile image_tile1 = ntp_tiles::NTPTile();
  image_tile1.title = base::ASCIIToUTF16("Title");
  image_tile1.url = GURL("http://image1.com");

  ntp_tiles::NTPTile image_tile2 = ntp_tiles::NTPTile();
  image_tile2.title = base::ASCIIToUTF16("Title");
  image_tile2.url = GURL("http://image2.com");

  id mock_favicon_fetcher = OCMClassMock([FaviconAttributesProvider class]);
  SetupMockCallback(mock_favicon_fetcher, {image_tile1.url, image_tile2.url},
                    {});

  ntp_tiles::NTPTilesVector tiles = {
      image_tile1,
  };

  ntp_tile_saver::SaveMostVisitedToDisk(tiles, mock_favicon_fetcher,
                                        TestFaviconDirectory());
  // Wait for all asynchronous tasks to complete.
  scoped_task_evironment_.RunUntilIdle();

  NSDictionary<NSURL*, NTPTile*>* saved_tiles =
      ntp_tile_saver::ReadSavedMostVisited();
  NSString* image_title1 = base::SysUTF16ToNSString(image_tile1.title);
  NSURL* image_url1 = net::NSURLWithGURL(image_tile1.url);
  NTPTile* saved_tile1 = [saved_tiles objectForKey:image_url1];
  VerifyWithImage(saved_tile1, image_title1, image_url1);

  ntp_tiles::NTPTilesVector tiles2 = {
      image_tile2,
  };

  ntp_tile_saver::SaveMostVisitedToDisk(tiles2, mock_favicon_fetcher,
                                        TestFaviconDirectory());
  // Wait for all asynchronous tasks to complete.
  scoped_task_evironment_.RunUntilIdle();
  NSDictionary<NSURL*, NTPTile*>* saved_tiles2 =
      ntp_tile_saver::ReadSavedMostVisited();
  NSString* image_title2 = base::SysUTF16ToNSString(image_tile2.title);
  NSURL* image_url2 = net::NSURLWithGURL(image_tile2.url);
  NTPTile* saved_tile2 = [saved_tiles2 objectForKey:image_url2];
  VerifyWithImage(saved_tile2, image_title2, image_url2);

  EXPECT_FALSE([[NSFileManager defaultManager]
      fileExistsAtPath:[[TestFaviconDirectory()
                           URLByAppendingPathComponent:saved_tile1
                                                           .faviconFileName]
                           path]]);
}

// Checks the different icon transition for an item.
// Checks that when a fallback exists, it persists even if an image is set.
// Checks that if a new icon is received it replaces the old one.
TEST_F(NTPTileSaverControllerTest, UpdateEntry) {
  ntp_tiles::NTPTile tile = ntp_tiles::NTPTile();

  // Set up a red favicon.
  tile.title = base::ASCIIToUTF16("Title");
  tile.url = GURL("http://url.com");
  NSString* ns_title = base::SysUTF16ToNSString(tile.title);
  NSURL* ns_url = net::NSURLWithGURL(tile.url);

  UIImage* red_image = CreateMockImage([UIColor redColor]);
  id mock_favicon_image_fetcher =
      OCMClassMock([FaviconAttributesProvider class]);
  SetupMockCallback(mock_favicon_image_fetcher, {tile.url}, {});
  id mock_favicon_fallback_fetcher =
      OCMClassMock([FaviconAttributesProvider class]);
  SetupMockCallback(mock_favicon_fallback_fetcher, {}, {tile.url});
  ntp_tiles::NTPTilesVector tiles = {
      tile,
  };
  ntp_tile_saver::SaveMostVisitedToDisk(tiles, mock_favicon_image_fetcher,
                                        TestFaviconDirectory());
  // Wait for all asynchronous tasks to complete.
  scoped_task_evironment_.RunUntilIdle();
  NSDictionary<NSURL*, NTPTile*>* saved =
      ntp_tile_saver::ReadSavedMostVisited();
  NTPTile* saved_tile = [saved objectForKey:ns_url];
  VerifyWithImage(saved_tile, ns_title, ns_url);

  // Update image to blue
  UIImage* blue_image = CreateMockImage([UIColor blueColor]);
  EXPECT_NSNE(UIImagePNGRepresentation(red_image),
              UIImagePNGRepresentation(blue_image));
  ntp_tile_saver::SaveMostVisitedToDisk(tiles, mock_favicon_image_fetcher,
                                        TestFaviconDirectory());
  // Wait for all asynchronous tasks to complete.
  scoped_task_evironment_.RunUntilIdle();
  saved = ntp_tile_saver::ReadSavedMostVisited();
  saved_tile = [saved objectForKey:ns_url];
  VerifyWithImage(saved_tile, ns_title, ns_url);

  // Update with fallback
  ntp_tile_saver::SaveMostVisitedToDisk(tiles, mock_favicon_fallback_fetcher,
                                        TestFaviconDirectory());
  // Wait for all asynchronous tasks to complete.
  scoped_task_evironment_.RunUntilIdle();
  saved = ntp_tile_saver::ReadSavedMostVisited();
  saved_tile = [saved objectForKey:ns_url];
  VerifyWithFallback(saved_tile, ns_title, ns_url);

  // Update image to green
  UIImage* green_image = CreateMockImage([UIColor greenColor]);
  EXPECT_NSNE(UIImagePNGRepresentation(blue_image),
              UIImagePNGRepresentation(green_image));
  ntp_tile_saver::SaveMostVisitedToDisk(tiles, mock_favicon_image_fetcher,
                                        TestFaviconDirectory());
  // Wait for all asynchronous tasks to complete.
  scoped_task_evironment_.RunUntilIdle();
  saved = ntp_tile_saver::ReadSavedMostVisited();
  saved_tile = [saved objectForKey:ns_url];
  // Fallback should still be present.
  VerifyWithFallbackAndImage(saved_tile, ns_title, ns_url);

  // Remove tile.
  ntp_tile_saver::SaveMostVisitedToDisk(ntp_tiles::NTPTilesVector(),
                                        mock_favicon_image_fetcher,
                                        TestFaviconDirectory());
  // Wait for all asynchronous tasks to complete.
  scoped_task_evironment_.RunUntilIdle();
  EXPECT_FALSE([[NSFileManager defaultManager]
      fileExistsAtPath:[[TestFaviconDirectory()
                           URLByAppendingPathComponent:saved_tile
                                                           .faviconFileName]
                           path]]);
}

}  // anonymous namespace
