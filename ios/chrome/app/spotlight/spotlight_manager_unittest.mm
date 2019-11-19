// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#import <CoreSpotlight/CoreSpotlight.h>
#import <Foundation/Foundation.h>

#include "base/location.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/large_icon_service_impl.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/spotlight/spotlight_provider.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;

const char kDummyIconUrl[] = "http://www.example.com/touch_icon.png";

favicon_base::FaviconRawBitmapResult CreateTestBitmap(int w, int h) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  CGRect rect = CGRectMake(0, 0, w, h);
  UIGraphicsBeginImageContext(rect.size);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextFillRect(context, rect);
  UIImage* favicon = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  NSData* png = UIImagePNGRepresentation(favicon);
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes(
      static_cast<const unsigned char*>([png bytes]), [png length]));

  result.bitmap_data = data;
  result.pixel_size = gfx::Size(w, h);
  result.icon_url = GURL(kDummyIconUrl);
  result.icon_type = favicon_base::IconType::kTouchIcon;
  CHECK(result.is_valid());
  return result;
}

class SpotlightManagerTest : public PlatformTest {
 protected:
  SpotlightManagerTest() {
    model_ = bookmarks::TestBookmarkClient::CreateModel();
    large_icon_service_.reset(new favicon::LargeIconServiceImpl(
        &mock_favicon_service_, /*image_fetcher=*/nullptr,
        /*desired_size_in_dip_for_server_requests=*/0,
        /*icon_type_for_server_requests=*/favicon_base::IconType::kTouchIcon,
        /*google_server_client_param=*/"test_chrome"));
    bookmarksSpotlightManager_ = [[BookmarksSpotlightManager alloc]
        initWithLargeIconService:large_icon_service_.get()
                   bookmarkModel:model_.get()];

    EXPECT_CALL(mock_favicon_service_,
                GetLargestRawFaviconForPageURL(_, _, _, _, _))
        .WillRepeatedly([](auto, auto, auto,
                           favicon_base::FaviconRawBitmapCallback callback,
                           base::CancelableTaskTracker* tracker) {
          return tracker->PostTask(
              base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
              base::BindOnce(std::move(callback), CreateTestBitmap(24, 24)));
        });
  }

  ~SpotlightManagerTest() override { [bookmarksSpotlightManager_ shutdown]; }

  base::test::TaskEnvironment task_environment_;
  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<favicon::LargeIconServiceImpl> large_icon_service_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  BookmarksSpotlightManager* bookmarksSpotlightManager_;
};

TEST_F(SpotlightManagerTest, testSpotlightID) {
  // Creating CSSearchableItem requires Spotlight to be available on the device.
  if (!spotlight::IsSpotlightAvailable())
    return;
  GURL url("http://url.com");
  NSString* spotlightId =
      [bookmarksSpotlightManager_ spotlightIDForURL:url title:@"title"];
  NSString* expectedSpotlightId =
      [NSString stringWithFormat:@"%@.9c6b643df2a0c990",
                                 spotlight::StringFromSpotlightDomain(
                                     spotlight::DOMAIN_BOOKMARKS)];
  EXPECT_NSEQ(spotlightId, expectedSpotlightId);
}

TEST_F(SpotlightManagerTest, testParentKeywordsForNode) {
  const bookmarks::BookmarkNode* root = model_->bookmark_bar_node();
  static const std::string model_string("a 1:[ b c ] d 2:[ 21:[ e ] f g ] h");
  bookmarks::test::AddNodesFromModelString(model_.get(), root, model_string);
  const bookmarks::BookmarkNode* eNode =
      root->children()[3]->children().front()->children().front().get();
  NSMutableArray* keywords = [[NSMutableArray alloc] init];
  [bookmarksSpotlightManager_ getParentKeywordsForNode:eNode inArray:keywords];
  EXPECT_EQ([keywords count], 2u);
  EXPECT_TRUE([[keywords objectAtIndex:0] isEqualToString:@"21"]);
  EXPECT_TRUE([[keywords objectAtIndex:1] isEqualToString:@"2"]);
}

TEST_F(SpotlightManagerTest, testBookmarksCreateSpotlightItemsWithUrl) {
  // Creating CSSearchableItem requires Spotlight to be available on the device.
  if (!spotlight::IsSpotlightAvailable())
    return;

  const bookmarks::BookmarkNode* root = model_->bookmark_bar_node();
  static const std::string model_string("a 1:[ b c ] d 2:[ 21:[ e ] f g ] h");
  bookmarks::test::AddNodesFromModelString(model_.get(), root, model_string);
  const bookmarks::BookmarkNode* eNode =
      root->children()[3]->children().front()->children().front().get();

  NSString* spotlightID = [bookmarksSpotlightManager_
      spotlightIDForURL:eNode->url()
                  title:base::SysUTF16ToNSString(eNode->GetTitle())];
  NSMutableArray* keywords = [[NSMutableArray alloc] init];
  [bookmarksSpotlightManager_ getParentKeywordsForNode:eNode inArray:keywords];
  NSArray* items = [bookmarksSpotlightManager_
      spotlightItemsWithURL:eNode->url()
                    favicon:nil
               defaultTitle:base::SysUTF16ToNSString(eNode->GetTitle())];
  EXPECT_TRUE([items count] == 1);
  CSSearchableItem* item = [items objectAtIndex:0];
  EXPECT_NSEQ([item uniqueIdentifier], spotlightID);
  EXPECT_NSEQ([[item attributeSet] title], @"e");
  EXPECT_NSEQ([[[item attributeSet] URL] absoluteString], @"http://e.com/");
  [bookmarksSpotlightManager_ addKeywords:keywords toSearchableItem:item];
  // We use the set intersection to verify that the item from the Spotlight
  // manager
  // contains all the newly added Keywords.
  NSMutableSet* spotlightManagerKeywords =
      [NSMutableSet setWithArray:[[item attributeSet] keywords]];
  NSSet* testModelKeywords = [NSSet setWithArray:keywords];
  [spotlightManagerKeywords intersectSet:testModelKeywords];
  EXPECT_NSEQ(spotlightManagerKeywords, testModelKeywords);
}

TEST_F(SpotlightManagerTest, testDefaultKeywordsExist) {
  // Creating CSSearchableItem requires Spotlight to be available on the device.
  if (!spotlight::IsSpotlightAvailable())
    return;

  const bookmarks::BookmarkNode* root = model_->bookmark_bar_node();
  static const std::string model_string("a 1:[ b c ] d 2:[ 21:[ e ] f g ] h");
  bookmarks::test::AddNodesFromModelString(model_.get(), root, model_string);
  const bookmarks::BookmarkNode* aNode = root->children().front().get();
  NSArray* items = [bookmarksSpotlightManager_
      spotlightItemsWithURL:aNode->url()
                    favicon:nil
               defaultTitle:base::SysUTF16ToNSString(aNode->GetTitle())];
  EXPECT_TRUE([items count] == 1);
  CSSearchableItem* item = [items objectAtIndex:0];
  NSSet* spotlightManagerKeywords =
      [NSSet setWithArray:[[item attributeSet] keywords]];
  EXPECT_TRUE([spotlightManagerKeywords count] > 0);
  // Check static/hardcoded keywords exist
  NSSet* hardCodedKeywordsSet =
      [NSSet setWithArray:ios::GetChromeBrowserProvider()
                              ->GetSpotlightProvider()
                              ->GetAdditionalKeywords()];
  EXPECT_TRUE([hardCodedKeywordsSet isSubsetOfSet:spotlightManagerKeywords]);
}
